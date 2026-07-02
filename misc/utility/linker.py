from misc.utility.color import print_info


def override_mold_linker_with_gold(env, module_name="build"):
    """
    Mold cannot link very large editor binaries (PC32/PLT32 relocation overflow).
    CI setup-mold symlinks ld.bfd to mold, so -fuse-ld=bfd still invokes mold.
    """
    if env.get("linker") != "mold":
        return False
    if any(isinstance(flag, str) and "-fuse-ld=gold" in flag for flag in env.get("LINKFLAGS", [])):
        return False
    env["LINKFLAGS"] = [
        flag
        for flag in env["LINKFLAGS"]
        if not (
            isinstance(flag, str)
            and ("-fuse-ld=mold" in flag or "-fuse-ld=bfd" in flag or (flag.startswith("-B") and "/mold" in flag))
        )
    ]
    env.Append(LINKFLAGS=["-fuse-ld=gold"])
    print_info(f"{module_name}: overriding mold linker with gold for final link")
    return True
