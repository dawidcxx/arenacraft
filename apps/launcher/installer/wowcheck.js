// Custom action embedded in Arenacraft.msi (JScript, Binary table).
//
// Rejects install directories that do not look like a World of Warcraft
// 3.3.5a client: Wow.exe and Data\common.MPQ must both be present. Windows
// filesystem lookups are case-insensitive, so `wow.exe`/`common.mpq` pass.

var msiMessageError = 0x01000000;

// Entry point, named by JScriptCall in Arenacraft.wxs. Run after CostFinalize,
// before RemoveExistingProducts/InstallFiles, so INSTALLDIR is the directory
// the user picked and a failure cannot uninstall the previous version.
function ValidateWowDir() {
    var problem = wowDirProblem(Session.Property("INSTALLDIR"));
    if (!problem) return 0;

    var record = Session.Installer.CreateRecord(0);
    record.StringData(0) = problem;
    Session.Message(msiMessageError, record);
    return 1;
}

// Split out so it can be exercised without an installer session.
function wowDirProblem(dir) {
    if (!dir) return "The installer did not pass an install directory.";

    var fso = new ActiveXObject("Scripting.FileSystemObject");
    if (!fso.FileExists(fso.BuildPath(dir, "Wow.exe"))) {
        return "Wow.exe was not found in:\n\n" + dir +
            "\n\nSelect the World of Warcraft 3.3.5a directory, the one that contains Wow.exe.";
    }
    if (!fso.FileExists(fso.BuildPath(fso.BuildPath(dir, "Data"), "common.MPQ"))) {
        return "Data\\common.MPQ was not found in:\n\n" + dir +
            "\n\nSelect the World of Warcraft 3.3.5a directory.";
    }
    return null;
}
