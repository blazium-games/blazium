import os
import sys
from typing import TYPE_CHECKING

from methods import print_error, print_info, print_warning
from platform_methods import validate_arch

if TYPE_CHECKING:
    from SCons.Script.SConscript import SConsEnvironment


def get_name():
    return "PS3"


def _get_ps3dev():
    return os.environ.get("PS3DEV", "/usr/local/ps3dev")


def _get_psl1ght():
    return os.environ.get("PSL1GHT", _get_ps3dev())


def _find_lv2_ld(ps3dev):
    candidates = [
        os.path.join(ps3dev, "ppu", "lib", "lv2.ld"),
        os.path.join(ps3dev, "ppu", "powerpc64-ps3-elf", "lib", "lv2.ld"),
    ]
    for path in candidates:
        if os.path.isfile(path):
            return path
    return ""


def can_build():
    ps3dev = _get_ps3dev()
    psl1ght = _get_psl1ght()

    if not os.path.isdir(ps3dev):
        print_error("PS3DEV not found at '%s'. PS3 platform disabled." % ps3dev)
        return False

    if not os.path.isdir(psl1ght):
        print_error("PSL1GHT not found at '%s'. PS3 platform disabled." % psl1ght)
        return False

    ppu_gcc = os.path.join(ps3dev, "ppu", "bin", "ppu-gcc")
    if not os.path.isfile(ppu_gcc) and os.system("ppu-gcc --version > /dev/null 2>&1") != 0:
        print_error("ppu-gcc not found. PS3 platform disabled.")
        return False

    if os.system("ppu-gcc --version 2>/dev/null | head -1 | grep -qE 'GCC\\) [0-8]\\.'") == 0:
        print_error(
            "ppu-gcc is older than GCC 9. Blazium requires a modern PS3 toolchain (gcc-9.5+). "
            "Build ps3toolchain from the Blazium fork."
        )
        return False

    if not _find_lv2_ld(psl1ght):
        print_error("PSL1GHT lv2.ld not found. PS3 platform disabled.")
        return False

    if not os.path.isdir(os.path.join(psl1ght, "ppu", "include")):
        print_error("PSL1GHT headers not found. PS3 platform disabled.")
        return False

    return True


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        BoolVariable("norsx", "Use NoRSX software framebuffer", True),
        BoolVariable("rsx", "Enable RSX/GCM hardware renderer (experimental)", False),
        BoolVariable("software_canvas", "Enable PS3 CPU canvas compositor (NoRSX)", True),
        BoolVariable("rpcs3_compat", "RPCS3 interpreter link compatibility (omit --gc-sections)", True),
        BoolVariable("spu_jobs", "Enable SPU job queue (PPU fallback in M9)", True),
    ]


def get_doc_classes():
    return [
        "EditorExportPlatformPS3",
        "PlayStation3",
    ]


def get_doc_path():
    return "doc_classes"


def get_flags():
    return {
        "arch": "ppc64",
        "tools": False,
        "vulkan": False,
        "opengl3": False,
        "use_sowrap": False,
        "module_raycast_enabled": False,
        "module_gdextension_enabled": False,
        "module_websocket_enabled": False,
        "module_autowork_enabled": False,
        "module_crowdcontrol_enabled": False,
        "module_obsclient_enabled": False,
        "module_socketio_enabled": False,
        "module_justamcp_enabled": False,
        "module_ps3_module_enabled": True,
        "module_jolt_physics_enabled": False,
        "module_navigation_enabled": False,
        "module_theora_enabled": False,
        "module_vorbis_enabled": False,
        "module_meshoptimizer_enabled": False,
        "disable_3d": True,
        "module_gltf_enabled": False,
        "module_fbx_enabled": False,
        "module_gridmap_enabled": False,
        "module_csg_enabled": False,
        "module_enet_server_enabled": False,
        "module_enet_enabled": False,
        "module_town_sdk_enabled": False,
        "module_multiuser_editor_enabled": False,
        "module_sqlite_enabled": False,
        "module_jsonrpc_enabled": False,
        "module_multiplayer_enabled": False,
        "module_astcenc_enabled": False,
        "module_bcdec_enabled": False,
        "module_bmp_enabled": False,
        "module_dds_enabled": False,
        "module_hdr_enabled": False,
        "module_tga_enabled": False,
        "module_regex_enabled": False,
        "module_clickertools_enabled": False,
        "module_dotcsv_enabled": False,
        "module_dotenv_enabled": False,
        "module_dotini_enabled": False,
        "module_httpserver_enabled": False,
        "module_ircclient_enabled": False,
        "module_jwttool_enabled": False,
        "module_kickapi_enabled": False,
        "module_rcon_enabled": False,
        "module_socialexports_enabled": False,
        "module_tiled_importer_enabled": False,
        "module_twitchapi_enabled": False,
        "module_goap_enabled": False,
        "module_interactive_music_enabled": False,
        "module_minimp3_enabled": False,
        "module_noise_enabled": False,
        "module_upnp_enabled": False,
        "module_webrtc_enabled": False,
        "module_ktx_enabled": False,
        "module_etcpak_enabled": False,
        "module_msdfgen_enabled": False,
        "module_ogg_enabled": False,
        "module_jpg_enabled": False,
        "minizip": False,
        "brotli": False,
        "builtin_zlib": False,
        "builtin_bullet": False,
        # Portlibs ships freetype 2.4.3; HarfBuzz needs 2.13+ APIs. Use engine builtin until portlibs is upgraded.
        "builtin_freetype": True,
        # Portlibs libpng is too old for ThorVG's png_image simplified API.
        "builtin_libpng": True,
        "builtin_libwebp": True,
        "builtin_mbedtls": False,
        "builtin_miniupnpc": False,
        "builtin_enet": False,
        "builtin_pcre2": True,
        "builtin_pcre2_with_jit": False,
        "optimize": "speed",
        # newlib/LV2 has no POSIX pthread gthr backend yet (libstdc++ uses gthr-single).
        "threads": False,
    }


def configure(env: "SConsEnvironment"):
    ps3dev = _get_ps3dev()
    psl1ght = _get_psl1ght()
    portlibs = os.path.join(ps3dev, "portlibs", "ppu")

    validate_arch(env["arch"], get_name(), ["ppc64"])

    ps3_include = env.Dir("#platform/ps3/include").srcnode().abspath
    env.Prepend(CCFLAGS=["-I%s" % ps3_include])
    env.Prepend(CXXFLAGS=["-I%s" % ps3_include, "-include", "platform/ps3/ps3_libstdcxx_compat.h"])

    env["ENV"]["PS3DEV"] = ps3dev
    env["ENV"]["PSL1GHT"] = psl1ght
    ppu_bin = os.path.join(ps3dev, "ppu", "bin")
    spu_bin = os.path.join(ps3dev, "spu", "bin")
    host_bin = os.path.join(ps3dev, "bin")
    updated_path = ":".join(
        [
            ppu_bin,
            spu_bin,
            host_bin,
            os.environ.get("PATH", ""),
        ]
    )
    env["ENV"]["PATH"] = updated_path
    os.environ["PATH"] = updated_path

    env["CC"] = "ppu-gcc"
    env["CXX"] = "ppu-g++"
    env["AR"] = "ppu-ar"
    env["RANLIB"] = "ppu-ranlib"
    env["LINK"] = "ppu-g++"

    env.Append(CPPPATH=["#platform/ps3"])
    env.Append(
        CPPPATH=[
            os.path.join(psl1ght, "ppu", "include"),
            os.path.join(ps3dev, "ppu", "powerpc64-ps3-elf", "include"),
            os.path.join(portlibs, "include"),
            os.path.join(portlibs, "include", "freetype2"),
        ]
    )
    env.Append(
        LIBPATH=[
            os.path.join(psl1ght, "ppu", "lib"),
            os.path.join(portlibs, "lib"),
        ]
    )

    env.Append(
        CPPDEFINES=[
            "PS3_ENABLED",
            "__PS3__",
            "__lv2ppu__",
            "__PPU__",
            "__powerpc64__",
            "UNIX_ENABLED",
            "UNIX_SOCKET_UNAVAILABLE",
            "PTHREAD_NO_RENAME",
            "HB_NO_MT",
            "JPH_PLATFORM_LINUX",
        ]
    )

    env.Append(CCFLAGS=["-mcpu=cell", "-ffunction-sections", "-fdata-sections", "-mno-fp-in-toc", "-mno-sum-in-toc", "-mminimal-toc"])
    env.Append(CXXFLAGS=["-fno-threadsafe-statics", "-fno-use-cxa-atexit"])
    env.Append(LINKFLAGS=["-Wl,-melf64ppc", "-Wl,--gc-sections", "-Wl,--relax"])
    if env.get("rpcs3_compat", True):
        env["LINKFLAGS"] = [flag for flag in env["LINKFLAGS"] if flag != "-Wl,--gc-sections"]
        env["CCFLAGS"] = [flag for flag in env["CCFLAGS"] if flag not in ("-ffunction-sections", "-fdata-sections")]
        print_info("PS3: rpcs3_compat enabled (--gc-sections disabled; use rpcs3_compat=no for RPCS3 LLVM).")
    else:
        print_info("PS3: rpcs3_compat disabled (--gc-sections enabled for RPCS3 LLVM jump tables).")

    if env.get("software_canvas", True):
        env.Append(CPPDEFINES=["SOFTWARE_CANVAS_ENABLED"])
        print_info("PS3: software canvas compositor enabled.")

    env.Append(LIBS=["sysbase", "c", "rt", "lv2", "io", "sysutil", "sysmodule"])

    norsx_lib = os.path.join(portlibs, "lib", "libnorsx.a")
    if not os.path.isfile(norsx_lib):
        norsx_lib = os.path.join(portlibs, "lib", "libNoRSX.a")
    if env["norsx"] and os.path.isfile(norsx_lib):
        env.Append(CPPDEFINES=["NORSX_ENABLED"])
        # NoRSX wraps RSX/GCM; link graphics libs even without the experimental RSX renderer.
        # NoRSX references sysutil; static link order requires sysutil after NoRSX.
        env.Append(LIBS=["NoRSX", "gcm_sys", "rsx", "sysutil"])
        print_info("PS3: NoRSX enabled from portlibs.")
    elif env["norsx"]:
        print_warning("PS3: NoRSX library not found; using software framebuffer stub.")

    ps3sound_lib = os.path.join(portlibs, "lib", "libps3sound.a")
    if not os.path.isfile(ps3sound_lib):
        ps3sound_lib = os.path.join(portlibs, "lib", "libspu_sound.a")
    if os.path.isfile(ps3sound_lib):
        env.Append(CPPDEFINES=["PS3SOUND_ENABLED"])
        if ps3sound_lib.endswith("libps3sound.a"):
            env.Append(LIBS=["ps3sound"])
        else:
            env.Append(LIBS=["spu_sound"])
        print_info("PS3: sound library enabled from portlibs.")

    if env["rsx"]:
        env.Append(CPPDEFINES=["RSX_ENABLED"])
        env.Append(LIBS=["gcm_sys", "rsx"])
        print_info("PS3: RSX/GCM hardware renderer enabled (experimental).")

    if env.get("spu_jobs", True):
        env.Append(CPPDEFINES=["SPU_JOBS_ENABLED"])
        print_info("PS3: SPU job queue enabled (PPU fallback until SPU ELF linked).")

    if not env["builtin_zlib"] and os.path.isfile(os.path.join(portlibs, "lib", "libz.a")):
        env.Append(LIBS=["z"])

    if not env["builtin_mbedtls"]:
        for lib in ("mbedtls", "mbedx509", "mbedcrypto"):
            if os.path.isfile(os.path.join(portlibs, "lib", "lib%s.a" % lib)):
                env.Append(LIBS=[lib])

    if os.path.isdir(os.path.join(portlibs, "lib", "pkgconfig")):
        env["ENV"]["PKG_CONFIG_PATH"] = os.path.join(portlibs, "lib", "pkgconfig")

    env.extra_suffix = ".ps3" + env.extra_suffix

    if env["debug_symbols"]:
        env.Append(CCFLAGS=["-g1"])
    else:
        env.Append(CCFLAGS=["-g0"])

    print_info("PS3 platform configured (PS3DEV=%s, PSL1GHT=%s)" % (ps3dev, psl1ght))
