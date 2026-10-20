const std = @import("std");

const EnginePaths = struct {
    const engine_src = "engine/src";
    const third_party = "third_party";
    const app_src = "app/src";
    const app_assets = "app/assets";
    const app_res = "app/res";
};

fn collectSources(
    b: *std.Build,
    allocator: std.mem.Allocator,
    cpp_out: *std.ArrayList([]const u8),
    c_out: *std.ArrayList([]const u8),
    dir: []const u8,
) !void {
    const io = b.graph.io;
    var d = try std.Io.Dir.cwd().openDir(io, dir, .{ .iterate = true });
    defer d.close(io);
    var walker = try d.walk(allocator);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        const ext = std.fs.path.extension(entry.path);
        const is_cpp = std.mem.eql(u8, ext, ".cpp");
        if (!is_cpp and !std.mem.eql(u8, ext, ".c")) continue;
        const full = try std.fs.path.join(allocator, &.{ dir, entry.path });
        defer allocator.free(full);
        const list = if (is_cpp) cpp_out else c_out;
        try list.append(allocator, b.dupe(full));
    }
}

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const is_windows = target.result.os.tag == .windows;

    var cpp_sources: std.ArrayList([]const u8) = .empty;
    var c_sources: std.ArrayList([]const u8) = .empty;

    var dirs: std.ArrayList([]const u8) = .empty;
    dirs.append(b.allocator, EnginePaths.engine_src ++ "/engine") catch @panic("OOM");
    dirs.append(b.allocator, EnginePaths.engine_src ++ "/editor") catch @panic("OOM");
    dirs.append(b.allocator, EnginePaths.engine_src ++ "/windows") catch @panic("OOM");
    dirs.append(b.allocator, EnginePaths.third_party ++ "/imgui") catch @panic("OOM");
    dirs.append(b.allocator, EnginePaths.third_party ++ "/implot") catch @panic("OOM");
    dirs.append(b.allocator, EnginePaths.third_party ++ "/bullet") catch @panic("OOM");
    if (optimize == .debug) {
        dirs.append(b.allocator, EnginePaths.engine_src ++ "/unit_tests") catch @panic("OOM");
    }
    for (dirs.items) |dir| {
        collectSources(b, b.allocator, &cpp_sources, &c_sources, dir) catch @panic("failed to list sources");
    }
    c_sources.append(b.allocator, b.dupe(EnginePaths.engine_src ++ "/gl.c")) catch @panic("OOM");
    cpp_sources.append(b.allocator, b.dupe(EnginePaths.app_src ++ "/main.cpp")) catch @panic("OOM");

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
        .link_libc = true,
    });

    mod.addIncludePath(b.path(EnginePaths.engine_src));
    mod.addIncludePath(b.path(EnginePaths.third_party));
    mod.addIncludePath(b.path(EnginePaths.third_party ++ "/bullet"));
    mod.addCMacro("NOMINMAX", "1");
    mod.addCMacro("_CRT_SECURE_NO_WARNINGS", "1");
    mod.addCMacro("EDITOR", "1");
    if (optimize == .debug) mod.addCMacro("DEBUG", "1");
    if (is_windows) {
        mod.addCMacro("_WIN32", "1");
        mod.addCMacro("_WIN64", "1");
    } else {
        mod.addCMacro("__LINUX__", "1");
    }

    mod.addCSourceFiles(.{
        .files = cpp_sources.items,
        .flags = &.{ "-std=c++17", "-fpermissive", "-Wno-everything" },
    });
    mod.addCSourceFiles(.{
        .files = c_sources.items,
        .flags = &.{"-std=c11"},
        .language = .c,
    });

    const exe = b.addExecutable(.{
        .name = "bones_editor",
        .root_module = mod,
    });

    exe.root_module.addLibraryPath(b.path(EnginePaths.third_party ++ "/libs"));
    if (is_windows) {
        exe.root_module.addLibraryPath(b.path(EnginePaths.third_party ++ "/libs/x64"));
        exe.root_module.linkSystemLibrary("SDL3", .{});
        exe.root_module.linkSystemLibrary("freetype", .{});
        exe.root_module.linkSystemLibrary("assimp-vc143-mt", .{});
        exe.root_module.linkSystemLibrary("libcurl_a", .{});
        exe.root_module.linkSystemLibrary("zlibstatic", .{});
        exe.root_module.linkSystemLibrary("opengl32", .{});
        exe.root_module.linkSystemLibrary("ws2_32", .{});
        exe.root_module.linkSystemLibrary("winmm", .{});
        exe.root_module.linkSystemLibrary("crypt32", .{});
        exe.root_module.linkSystemLibrary("wldap32", .{});
        exe.root_module.linkSystemLibrary("shell32", .{});
        exe.root_module.linkSystemLibrary("ole32", .{});
        exe.root_module.linkSystemLibrary("uuid", .{});
        exe.root_module.linkSystemLibrary("user32", .{});
        exe.root_module.linkSystemLibrary("gdi32", .{});
        exe.root_module.linkSystemLibrary("advapi32", .{});
        exe.root_module.linkSystemLibrary("comdlg32", .{});
        exe.root_module.linkSystemLibrary("dbghelp", .{});
        exe.root_module.linkSystemLibrary("psapi", .{});
    } else {
        exe.root_module.linkSystemLibrary("SDL3", .{});
        exe.root_module.linkSystemLibrary("freetype", .{});
        exe.root_module.linkSystemLibrary("curl", .{});
        exe.root_module.linkSystemLibrary("ssl", .{});
        exe.root_module.linkSystemLibrary("crypto", .{});
        exe.root_module.linkSystemLibrary("assimp", .{});
        exe.root_module.linkSystemLibrary("dl", .{});
        exe.root_module.linkSystemLibrary("pthread", .{});
        exe.root_module.addRPathSpecial("$ORIGIN");
        b.installFile("third_party/libs/libassimp.so.5.4.3", "bin/libassimp.so.5");
    }

    b.installArtifact(exe);

    // Assets are loaded relative to the executable.
    const assets = [_][]const u8{ "engine_assets", "public_engine_assets", "icons", "vscodeSample", "plugins" };
    for (assets) |asset| {
        b.installDirectory(.{
            .source_dir = b.path(b.fmt("{s}/{s}", .{ EnginePaths.app_assets, asset })),
            .install_dir = .bin,
            .install_subdir = asset,
        });
    }
    b.installFile(b.fmt("{s}/Roboto Regular.ttf", .{EnginePaths.app_res}), "bin/Roboto Regular.ttf");

    if (is_windows) {
        const dlls = [_][]const u8{ "SDL3.dll", "freetype.dll", "assimp-vc143-mt.dll" };
        for (dlls) |dll| {
            b.installFile(b.fmt("{s}/libs/x64/{s}", .{ EnginePaths.third_party, dll }), b.fmt("bin/{s}", .{dll}));
        }
    }

    const run_step = b.step("run", "Run the engine/editor");
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    run_cmd.addPassthruArgs();
    run_step.dependOn(&run_cmd.step);
}
