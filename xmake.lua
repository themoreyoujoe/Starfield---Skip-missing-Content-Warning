-- MojoSkipMissingContent SFSE plugin
--
-- Build (from this folder):
--   ..\xmake-3.0.6-bin\xmake\xmake.exe config -m releasedbg -y
--   ..\xmake-3.0.6-bin\xmake\xmake.exe build -y
-- The DLL lands in build\windows\x64\releasedbg\MojoSkipMissingContent.dll.

set_xmakever("3.0.0")

includes("../commonlibsf")

set_project("MojoSkipMissingContent")
set_version("1.0.0")
set_arch("x64")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

add_rules("mode.debug", "mode.releasedbg")

target("MojoSkipMissingContent", function()
    add_rules("commonlibsf.plugin", {
        name = "MojoSkipMissingContent",
        author = "Mojo",
        description = "Skips the missing content and vanilla-save-with-mods prompts when loading a save"
    })

    add_files("src/**.cpp")
    add_includedirs("src")
end)
