# PlatformIO extra script: merge bootloader + partition table + app (+ LittleFS)
# into one image that Wokwi can load (.pio/build/<env>/firmware.merged.bin).
Import("env")
from os.path import join, isfile

# Offset of the LittleFS partition in the default 4MB Arduino table
# (see framework-arduinoespressif32/tools/partitions/default.csv).
FS_OFFSET = "0x290000"


def _fs_image_path(env):
    build_dir = env.subst("$BUILD_DIR")
    name = env.subst("$ESP32_FS_IMAGE_NAME") or "littlefs"
    for candidate in (name + ".bin", "littlefs.bin"):
        path = join(build_dir, candidate)
        if isfile(path):
            return path
    return join(build_dir, "littlefs.bin")


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

    fs_image = _fs_image_path(env)
    if isfile(fs_image):
        images.append((FS_OFFSET, fs_image))
        print("Including filesystem image: %s @ %s" % (fs_image, FS_OFFSET))
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
