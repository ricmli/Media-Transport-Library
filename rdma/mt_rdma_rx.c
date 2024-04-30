/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "mt_rdma.h"

static int rdma_rx_send_buffer_done(struct mt_rdma_rx_ctx* ctx, uint16_t idx) {
  struct mt_rdma_rx_buffer* rx_buffer = &ctx->rx_buffers[idx];
  struct mt_rdma_message msg = {
      .magic = MT_RDMA_MSG_MAGIC,
      .type = MT_RDMA_MSG_BUFFER_DONE,
      .buf_done.buf_idx = idx,
      .buf_done.seq_num = 0, /* todo */
      .buf_done.rx_buf_addr = (uint64_t)rx_buffer->buffer.addr,
      .buf_done.rx_buf_key = rx_buffer->mr->rkey,
  };
  int ret = rdma_post_send(ctx->id, NULL, &msg, sizeof(msg), NULL, IBV_SEND_INLINE);
  if (ret) {
    fprintf(stderr, "rdma_post_send failed\n");
    return -EIO;
  }
  /* post recv for next ready msg */
  void* r_msg = ctx->message_region + idx * 1024;
  ret = rdma_post_recv(ctx->id, r_msg, r_msg, 1024, ctx->message_mr);
  if (ret) {
    fprintf(stderr, "rdma_post_recv failed\n");
    return -EIO;
  }
  rx_buffer->status = MT_RDMA_BUFFER_STATUS_FREE;
  return 0;
}

static int rdma_rx_uinit_mrs(struct mt_rdma_rx_ctx* ctx) {
  if (ctx->message_mr) {
    ibv_dereg_mr(ctx->message_mr);
  }

  for (int i = 0; i < ctx->buffer_cnt; i++) {
    struct mt_rdma_rx_buffer* rx_buffer = &ctx->rx_buffers[i];
    if (rx_buffer->mr) ibv_dereg_mr(rx_buffer->mr);
  }

  return 0;
}

static int rdma_rx_init_mrs(struct mt_rdma_rx_ctx* ctx) {
  for (int i = 0; i < ctx->buffer_cnt; i++) {
    struct mt_rdma_rx_buffer* rx_buffer = &ctx->rx_buffers[i];
    struct ibv_mr* mr =
        ibv_reg_mr(ctx->pd, rx_buffer->buffer.addr, rx_buffer->buffer.capacity,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (!mr) {
      fprintf(stderr, "ibv_reg_mr failed\n");
      rdma_rx_uinit_mrs(ctx);
      return -ENOMEM;
    }
    rx_buffer->mr = mr;
  }

  struct ibv_mr* mr = ibv_reg_mr(ctx->pd, ctx->message_region, ctx->buffer_cnt * 1024,
                                 IBV_ACCESS_LOCAL_WRITE);
  if (!mr) {
    fprintf(stderr, "ibv_reg_mr failed\n");
    rdma_rx_uinit_mrs(ctx);
    return -ENOMEM;
  }
  ctx->message_mr = mr;

  return 0;
}

static int rdma_rx_free_buffers(struct mt_rdma_rx_ctx* ctx) {
  rdma_rx_uinit_mrs(ctx);
  if (ctx->message_region) free(ctx->message_region);
  if (ctx->rx_buffers) free(ctx->rx_buffers);

  return 0;
}

static int rdma_rx_alloc_buffers(struct mt_rdma_rx_ctx* ctx) {
  struct mtl_rdma_rx_ops* ops = &ctx->ops;
  ctx->buffer_cnt = ops->num_buffers;
  ctx->rx_buffers = (struct mt_rdma_rx_buffer*)calloc(ctx->buffer_cnt,
                                                      sizeof(struct mt_rdma_rx_buffer));
  if (!ctx->rx_buffers) {
    fprintf(stderr, "calloc failed\n");
    return -ENOMEM;
  }

  for (int i = 0; i < ctx->buffer_cnt; i++) {
    struct mt_rdma_rx_buffer* rx_buffer = &ctx->rx_buffers[i];
    rx_buffer->idx = i;
    rx_buffer->status = MT_RDMA_BUFFER_STATUS_FREE;
    rx_buffer->buffer.addr = ops->buffers[i];
    rx_buffer->buffer.capacity = ops->buffer_capacity;
  }

  /* alloc message region */
  ctx->message_region = (char*)calloc(ctx->buffer_cnt, 1024);
  if (!ctx->message_region) {
    fprintf(stderr, "calloc failed\n");
    rdma_rx_free_buffers(ctx);
    return -ENOMEM;
  }

  return 0;
}

/* cq poll thread */
static void* rdma_rx_cq_poll_thread(void* arg) {
  int ret = 0;
  struct mt_rdma_rx_ctx* ctx = arg;
  struct mtl_rdma_rx_ops* ops = &ctx->ops;
  struct ibv_wc wc;
  for (;;) {
    struct ibv_cq* cq;
    void* cq_ctx = NULL;
    ret = ibv_get_cq_event(ctx->cc, &cq, &cq_ctx);
    if (ret) {
      fprintf(stderr, "ibv_get_cq_event failed\n");
      goto out;
    }
    ibv_ack_cq_events(cq, 1);
    ret = ibv_req_notify_cq(cq, 0);
    if (ret) {
      fprintf(stderr, "ibv_req_notify_cq failed\n");
      goto out;
    }
    while (ibv_poll_cq(ctx->cq, 1, &wc)) {
      if (wc.status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Work completion error: %s\n", ibv_wc_status_str(wc.status));
        /* check more info */
        fprintf(stderr, "wc.opcode = %d, wc.vendor_error = 0x%x, wc.qp_num = %u\n",
                wc.opcode, wc.vendor_err, wc.qp_num);
        goto out;
      }
      if (wc.opcode == IBV_WC_RECV) {
        struct mt_rdma_message* msg = (struct mt_rdma_message*)wc.wr_id;
        if (msg->magic == MT_RDMA_MSG_MAGIC) {
          if (msg->type == MT_RDMA_MSG_BUFFER_READY) {
            uint16_t idx = msg->buf_ready.buf_idx;
            struct mt_rdma_rx_buffer* rx_buffer = &ctx->rx_buffers[idx];
            rx_buffer->status = MT_RDMA_BUFFER_STATUS_READY;
            ctx->stat_buffer_received++;
            /* what about other meta? */
            if (ops->notify_buffer_ready) {
              ret = ops->notify_buffer_ready(ops->priv, &rx_buffer->buffer);
              if (ret) {
                fprintf(stderr, "notify_buffer_ready failed\n");
                /* todo error handle */
              }
            }
          }
        }
      }
    }
  }

out:
  return NULL;
}

/* connect thread */
static void* rdma_rx_connect_thread(void* arg) {
  struct mt_rdma_rx_ctx* ctx = arg;
  struct rdma_cm_event* event;
  struct pollfd pfd;
  pfd.fd = ctx->ec->fd;
  pfd.events = POLLIN;

  while (!ctx->connect_stop) {
    int ret = poll(&pfd, 1, 200);
    if (ret > 0) {
      ret = rdma_get_cm_event(ctx->ec, &event);
      if (!ret) {
        switch (event->event) {
          case RDMA_CM_EVENT_ADDR_RESOLVED:
            ret = rdma_resolve_route(ctx->id, 2000);
            if (ret) {
              fprintf(stderr, "rdma_resolve_route failed\n");
              goto connect_err;
            }
            break;
          case RDMA_CM_EVENT_ROUTE_RESOLVED:
            ctx->pd = ibv_alloc_pd(event->id->verbs);
            if (!ctx->pd) {
              fprintf(stderr, "ibv_alloc_pd failed\n");
              goto connect_err;
            }
            ctx->cc = ibv_create_comp_channel(event->id->verbs);
            if (!ctx->cc) {
              fprintf(stderr, "ibv_create_comp_channel failed\n");
              goto connect_err;
            }
            ctx->cq = ibv_create_cq(event->id->verbs, 10, ctx, ctx->cc, 0);
            if (!ctx->cq) {
              fprintf(stderr, "ibv_create_cq failed\n");
              goto connect_err;
            }
            ret = ibv_req_notify_cq(ctx->cq, 0);
            if (ret) {
              fprintf(stderr, "ibv_req_notify_cq failed\n");
              goto connect_err;
            }
            struct ibv_qp_init_attr init_qp_attr = {
                .cap.max_send_wr = 10,
                .cap.max_recv_wr = 10,
                .cap.max_send_sge = 2,     /* gather message and meta */
                .cap.max_recv_sge = 2,     /* scatter message and meta */
                .cap.max_inline_data = 64, /* max mtu */
                .send_cq = ctx->cq,
                .recv_cq = ctx->cq,
                .qp_type = IBV_QPT_RC,
            };
            ret = rdma_create_qp(event->id, ctx->pd, &init_qp_attr);
            if (ret) {
              fprintf(stderr, "rdma_create_qp failed\n");
              goto connect_err;
            }
            ctx->qp = event->id->qp;
            ret = rdma_rx_init_mrs(ctx);
            if (ret) {
              fprintf(stderr, "rdma_tx_init_mrs failed\n");
              goto connect_err;
            }
            struct rdma_conn_param conn_param = {
                .initiator_depth = 1,
                .responder_resources = 1,
                .rnr_retry_count = 7 /* infinite retry */,
            };
            ret = rdma_connect(event->id, &conn_param);
            if (ret) {
              fprintf(stderr, "rdma_connect failed\n");
              goto connect_err;
            }
            break;
          case RDMA_CM_EVENT_ESTABLISHED:
            for (uint16_t i = 0; i < ctx->buffer_cnt; i++) /* start receiving */
              rdma_rx_send_buffer_done(ctx, i);
            ctx->connected = 1;

            ctx->cq_poll_stop = false;
            ret = pthread_create(&ctx->cq_poll_thread, NULL, rdma_rx_cq_poll_thread, ctx);
            if (ret) {
              fprintf(stderr, "pthread_create failed\n");
              goto connect_err;
            }
            break;
          case RDMA_CM_EVENT_ADDR_ERROR:
          case RDMA_CM_EVENT_ROUTE_ERROR:
          case RDMA_CM_EVENT_CONNECT_ERROR:
          case RDMA_CM_EVENT_UNREACHABLE:
          case RDMA_CM_EVENT_REJECTED:
            fprintf(stderr, "event: %s, error: %d\n", rdma_event_str(event->event),
                    event->status);
            break;
          default:
            break;
        }
        rdma_ack_cm_event(event);
      }
    } else if (ret == 0) {
      /* poll timeout */
    } else {
      break;
    }
  }

  return NULL;

connect_err:
  rdma_ack_cm_event(event);
  /* add more error handling */
  return NULL;
}

struct mtl_rdma_buffer* mtl_rdma_rx_get_buffer(mtl_rdma_rx_handle handle) {
  struct mt_rdma_rx_ctx* ctx = handle;
  if (!ctx->connected) {
    return NULL;
  }
  /* find a ready buffer */
  for (int i = 0; i < ctx->buffer_cnt; i++) {
    struct mt_rdma_rx_buffer* rx_buffer = &ctx->rx_buffers[i];
    if (rx_buffer->status == MT_RDMA_BUFFER_STATUS_READY) {
      rx_buffer->status = MT_RDMA_BUFFER_STATUS_IN_CONSUMPTION;
      return &rx_buffer->buffer;
    }
  }

  return NULL;
}

int mtl_rdma_rx_put_buffer(mtl_rdma_rx_handle handle, struct mtl_rdma_buffer* buffer) {
  struct mt_rdma_rx_ctx* ctx = handle;
  if (!ctx->connected) {
    return -1;
  }

  for (int i = 0; i < ctx->buffer_cnt; i++) {
    struct mt_rdma_rx_buffer* rx_buffer = &ctx->rx_buffers[i];
    if (&rx_buffer->buffer == buffer) {
      return rdma_rx_send_buffer_done(ctx, rx_buffer->idx);
    }
  }

  return -1;
}

int mtl_rdma_rx_free(mtl_rdma_rx_handle handle) {
  struct mt_rdma_rx_ctx* ctx = handle;

  if (!ctx) {
    return 0;
  }

  if (ctx->cq_poll_thread) {
    ctx->cq_poll_stop = true;
    pthread_join(ctx->cq_poll_thread, NULL);
  }

  if (ctx->connect_thread) {
    ctx->connect_stop = true;
    pthread_join(ctx->connect_thread, NULL);
  }

  if (ctx->id) {
    rdma_destroy_qp(ctx->id);
  }

  if (ctx->cq) {
    ibv_destroy_cq(ctx->cq);
  }

  if (ctx->cc) {
    ibv_destroy_comp_channel(ctx->cc);
  }

  if (ctx->pd) {
    ibv_dealloc_pd(ctx->pd);
  }

  if (ctx->ec) {
    rdma_destroy_event_channel(ctx->ec);
  }

  if (ctx->id) {
    rdma_destroy_id(ctx->id);
  }

  rdma_rx_free_buffers(ctx);

  return 0;
}

mtl_rdma_rx_handle mtl_rdma_rx_create(mtl_rdma_handle mrh, struct mtl_rdma_rx_ops* ops) {
  int ret = 0;
  struct mt_rdma_rx_ctx* ctx = calloc(1, sizeof(*ctx));
  if (!ctx) {
    fprintf(stderr, "malloc mt_rdma_rx_ctx failed\n");
    return NULL;
  }
  ctx->ops = *ops;

  ret = rdma_rx_alloc_buffers(ctx);
  if (ret) {
    fprintf(stderr, "rdma_rx_alloc_buffers failed\n");
    goto out;
  }

  ctx->ec = rdma_create_event_channel();
  if (!ctx->ec) {
    fprintf(stderr, "rdma_create_event_channel failed\n");
    goto out;
  }

  ret = rdma_create_id(ctx->ec, &ctx->id, ctx, RDMA_PS_TCP);
  if (ret) {
    fprintf(stderr, "rdma_create_id failed\n");
    goto out;
  }

  struct rdma_addrinfo hints = {};
  struct rdma_addrinfo *res, *rai;
  hints.ai_port_space = RDMA_PS_TCP;
  hints.ai_flags = RAI_PASSIVE;
  ret = rdma_getaddrinfo(ops->local_ip, NULL, &hints, &res);
  if (ret) {
    fprintf(stderr, "rdma_getaddrinfo failed\n");
    goto out;
  }
  hints.ai_src_addr = res->ai_src_addr;
  hints.ai_src_len = res->ai_src_len;
  hints.ai_flags &= ~RAI_PASSIVE;
  ret = rdma_getaddrinfo(ops->ip, ops->port, &hints, &rai);
  rdma_freeaddrinfo(res);
  if (ret) {
    fprintf(stderr, "rdma_getaddrinfo failed\n");
    goto out;
  }

  ret = rdma_resolve_addr(ctx->id, rai->ai_src_addr, rai->ai_dst_addr, 2000);
  rdma_freeaddrinfo(rai);
  if (ret) {
    fprintf(stderr, "rdma_resolve_addr failed\n");
    goto out;
  }

  ctx->connect_stop = false;
  ret = pthread_create(&ctx->connect_thread, NULL, rdma_rx_connect_thread, ctx);
  if (ret) {
    fprintf(stderr, "pthread_create failed\n");
    goto out;
  }

  return ctx;

out:
  mtl_rdma_rx_free(ctx);
  return NULL;
}
