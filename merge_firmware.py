# PlatformIO extra script: merge bootloader + partition table + app (+ LittleFS)
# into one image that Wokwi can load (.pio/build/<env>/firmware.merged.bin).
Import("env")
from os.path import join, isfile, basename


def _fs_image_path(env):
    build_dir = env.subst("$BUILD_DIR")
    name = env.subst("$ESP32_FS_IMAGE_NAME") or "littlefs"
    for candidate in (name + ".bin", "littlefs.bin"):
        path = join(build_dir, candidate)
        if isfile(path):
            return path
    return join(build_dir, "littlefs.bin")


def _partition_csv_path(env):
    """Resolve the active partitions CSV (project-relative or framework default)."""
    pt = env.BoardConfig().get("build.partitions", "default.csv")
    project_dir = env.subst("$PROJECT_DIR")
    candidates = [
        join(project_dir, pt),
        join(project_dir, "partitions", basename(pt)),
    ]
    # PlatformIO may already expand to an absolute path
    if pt and (pt.endswith(".csv") or "\\" in pt or "/" in pt):
        candidates.insert(0, pt)
    try:
        pkg = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
        if pkg:
            candidates.append(join(pkg, "tools", "partitions", basename(pt)))
    except Exception:
        pass
    for path in candidates:
        if path and isfile(path):
            return path
    return None


def _fs_offset(env):
    """Read LittleFS/SPIFFS offset from the active partition table (default 0x290000)."""
    csv_path = _partition_csv_path(env)
    if csv_path:
        with open(csv_path, "r", encoding="utf-8") as fh:
            for raw in fh:
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                parts = [p.strip() for p in line.split(",")]
                if len(parts) < 5:
                    continue
                name, _typ, subtype = parts[0].lower(), parts[1].lower(), parts[2].lower()
                if subtype in ("spiffs", "littlefs") or name in ("spiffs", "littlefs", "ffat"):
                    return parts[3]
    return "0x290000"


def merge_firmware(source, target, env):
    board = env.BoardConfig()
    build_dir = env.subst("$BUILD_DIR")
    merged = join(build_dir, "firmware.merged.bin")
    pkg = env.PioPlatform().get_package_dir("tool-esptoolpy")
    esptool = join(pkg, "esptool.py")
    if not isfile(esptool):
        esptool = join(pkg, "esptool")

    flash_size = board.get("upload.flash_size", "4MB")
    images = [(offset, env.subst(path)) for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET"), str(target[0])))

    fs_offset = _fs_offset(env)
    fs_image = _fs_image_path(env)
    if isfile(fs_image):
        images.append((fs_offset, fs_image))
        print("Including filesystem image: %s @ %s" % (fs_image, fs_offset))
    else:
        print(
            "Warning: filesystem image not found at %s — run "
            "`pio run -e %s -t buildfs` first" % (fs_image, env["PIOENV"])
        )

    cmd = [
        '"$PYTHONEXE"',
        '"%s"' % esptool,
        "--chip",
        board.get("build.mcu", "esp32"),
        "merge_bin",
        "-o",
        '"%s"' % merged,
        "--flash_mode",
        board.get("build.flash_mode", "dio"),
        "--flash_size",
        flash_size,
        "--fill-flash-size",
        flash_size,
    ]
    for offset, path in images:
        cmd += [offset if isinstance(offset, str) else hex(offset), '"%s"' % path]

    env.Execute(env.VerboseAction(" ".join(cmd), "Merging firmware images into %s" % merged))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)
