# thumbgen - video thumbnail extractor

`thumbgen` extracts a frame from a video at a timestamp using the FFmpeg C API.

## Usage

```bash
thumbgen VIDEO TIMESTAMP [OUTPUT]
```

`TIMESTAMP` accepts `SS`, `MM:SS`, or `HH:MM:SS`.

## Examples

```bash
# write video.thumb.png in current directory
thumbgen video.mp4 00:01:30

# choose output file explicitly
thumbgen clip.mkv 90 thumb.png
```

## Notes

- Built only when FFmpeg development libraries are available (`libavformat`, `libavcodec`, `libavutil`, `libswscale`).
