# Rust

Bindings for IMTL in Rust.

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
imtl = "0.1.2"
```

## Example

Simple program to use IMTL to send raw YUV frame from file.

```bash
cargo run --example video-tx -- --yuv /tmp/test.yuv --width 1920 --height 1080 --fps 30 --format yuv_422_8bit
# Check more options with --help
cargo run --example video-tx -- --help
```

Simple program to use IMTL to receive raw YUV frame and display or save the latest one to file.

```bash
cargo run --example video-rx -- --display --width 1920 --height 1080 --fps 30 --format yuv_422_8bit [--yuv /tmp/save.yuv]
# Check more options with --help
cargo run --example video-rx -- --help
```