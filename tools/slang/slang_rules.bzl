"""Small hermetic rules for Gargantuan-Belly's pinned Slang compiler."""

SlangLibraryInfo = provider(fields = ["sources"])

def _slang_library_impl(ctx):
    transitive = [dependency[SlangLibraryInfo].sources for dependency in ctx.attr.deps]
    sources = depset(ctx.files.srcs, transitive = transitive)
    return [
        DefaultInfo(files = sources),
        SlangLibraryInfo(sources = sources),
    ]

slang_library = rule(
    implementation = _slang_library_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = [".slang"]),
        "deps": attr.label_list(providers = [SlangLibraryInfo]),
    },
)

def _common_inputs(ctx):
    transitive = [dependency[SlangLibraryInfo].sources for dependency in ctx.attr.deps]
    return depset([ctx.file.src], transitive = transitive)

def _slang_cpp_impl(ctx):
    generated_cpp = ctx.actions.declare_file(ctx.attr.output_name + ".cpp")
    generated_hpp = ctx.actions.declare_file(ctx.attr.output_name + ".hpp")
    inputs = _common_inputs(ctx)

    for target, output in [("cpp", generated_cpp), ("hpp", generated_hpp)]:
        raw_output = ctx.actions.declare_file(
            ctx.attr.output_name + ".raw." + target,
        )
        arguments = ctx.actions.args()
        arguments.add(ctx.file.src.path)
        for import_path in ctx.attr.import_paths:
            arguments.add("-I", import_path)
        arguments.add("-D", "GARGANTUA_HOST_DOUBLE=1")
        arguments.add("-target", target)
        arguments.add("-matrix-layout-row-major")
        arguments.add("-O2")
        arguments.add("-g0")
        arguments.add("-warnings-as-errors", "all")
        arguments.add("-o", raw_output.path)
        ctx.actions.run(
            executable = ctx.file._slangc,
            arguments = [arguments],
            inputs = inputs,
            tools = ctx.attr._runtime[DefaultInfo].files,
            outputs = [raw_output],
            mnemonic = "SlangHostCodegen",
            progress_message = "Compiling canonical physics for the host",
        )
        # slangc deliberately emits an absolute path to its C++ prelude. Make
        # that include repository-relative so the generated source remains
        # hermetic, cacheable, and usable in a later Bazel action.
        ctx.actions.run(
            executable = ctx.executable._normalizer,
            inputs = [raw_output],
            outputs = [output],
            arguments = [raw_output.path, output.path],
            mnemonic = "NormalizeSlangPrelude",
        )

    return [
        DefaultInfo(files = depset([generated_cpp, generated_hpp])),
        OutputGroupInfo(
            cpp = depset([generated_cpp]),
            hpp = depset([generated_hpp]),
        ),
    ]

slang_cpp = rule(
    implementation = _slang_cpp_impl,
    attrs = {
        "src": attr.label(mandatory = True, allow_single_file = [".slang"]),
        "deps": attr.label_list(providers = [SlangLibraryInfo]),
        "import_paths": attr.string_list(mandatory = True),
        "output_name": attr.string(mandatory = True),
        "_slangc": attr.label(
            default = "@slang_toolchain//:bin/slangc",
            allow_single_file = True,
            cfg = "exec",
        ),
        "_runtime": attr.label(
            default = "@slang_toolchain//:toolchain_files",
            cfg = "exec",
        ),
        "_normalizer": attr.label(
            default = "//tools/slang:normalize_prelude",
            executable = True,
            cfg = "exec",
        ),
    },
)

def _slang_spirv_impl(ctx):
    output = ctx.actions.declare_file(ctx.attr.output_name)
    reflection = ctx.actions.declare_file(
        ctx.attr.output_name + ".reflection.json",
    )
    arguments = ctx.actions.args()
    arguments.add(ctx.file.src.path)
    for import_path in ctx.attr.import_paths:
        arguments.add("-I", import_path)
    arguments.add("-entry", ctx.attr.entry_point)
    arguments.add("-stage", ctx.attr.stage)
    arguments.add("-target", "spirv")
    arguments.add("-profile", ctx.attr.profile)
    arguments.add("-emit-spirv-directly")
    arguments.add("-matrix-layout-row-major")
    arguments.add("-O2")
    arguments.add("-g0")
    arguments.add("-warnings-as-errors", "all")
    arguments.add("-reflection-json", reflection.path)
    arguments.add("-o", output.path)
    ctx.actions.run(
        executable = ctx.file._slangc,
        arguments = [arguments],
        inputs = _common_inputs(ctx),
        tools = ctx.attr._runtime[DefaultInfo].files,
        outputs = [output, reflection],
        mnemonic = "SlangSpirv",
        progress_message = "Compiling canonical physics for Vulkan",
    )
    return [
        DefaultInfo(files = depset([output])),
        OutputGroupInfo(reflection = depset([reflection])),
    ]

slang_spirv = rule(
    implementation = _slang_spirv_impl,
    attrs = {
        "src": attr.label(mandatory = True, allow_single_file = [".slang"]),
        "deps": attr.label_list(providers = [SlangLibraryInfo]),
        "import_paths": attr.string_list(mandatory = True),
        "entry_point": attr.string(mandatory = True),
        "stage": attr.string(mandatory = True),
        "profile": attr.string(default = "spirv_1_3"),
        "output_name": attr.string(mandatory = True),
        "_slangc": attr.label(
            default = "@slang_toolchain//:bin/slangc",
            allow_single_file = True,
            cfg = "exec",
        ),
        "_runtime": attr.label(
            default = "@slang_toolchain//:toolchain_files",
            cfg = "exec",
        ),
    },
)
