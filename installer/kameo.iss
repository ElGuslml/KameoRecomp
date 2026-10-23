; KameoRecomp's installer (Inno Setup 6.6 or later). recomp-tools' tools\release\Package-Release.ps1 builds it from the
; release folder it has just made, when Inno Setup is installed:
;   ISCC /DAppVersion=0.2.0 /DSourceDir=<release folder> /DOutputDir=<folder> installer\kameo.iss
;
; - It installs only this project's files (the release folder: the executables, the DLLs, the documents, the
;   licenses, assets\README.md). The game's files come from the player's ISO: the launcher asks for it on its first
;   run.
; - For the current user, without administrator rights, in %LOCALAPPDATA%\Programs\KameoRecomp: the game writes its
;   saves next to itself (userdata\) and the launcher updates the files in place, which Program Files would not allow.
; - Installing a newer version over an installation (or over a portable folder) keeps the saves, the settings and the
;   game files.
; - Uninstalling asks before deleting the saves, achievements and settings, and before deleting the game files.
; POTCAWERecomp's installer, made general (recomp-tools' template).

#ifndef AppVersion
  #error Define AppVersion, SourceDir and OutputDir (see the top of this file)
#endif

#define AppName "Kameo: Elements of Power"
#define ShortcutName "Kameo - Elements of Power"
#define LauncherExe "Kameo Launcher.exe"
; The project's public repository, once there is one ("https://github.com/<user>/KameoRecomp"): the links of the
; uninstall entry.
#define RepoUrl ""

[Setup]
; Never change the AppId: Windows knows the installation by it, and a newer installer finds the older one with it.
AppId={{49C1A137-6F8E-4B87-9E57-6678A9F61981}
AppName={#AppName} (Recomp)
AppVersion={#AppVersion}
AppVerName={#AppName} (Recomp) {#AppVersion}
AppPublisher=Gustavo Garcia
#if RepoUrl != ""
AppPublisherURL={#RepoUrl}
AppSupportURL={#RepoUrl}/issues
AppUpdatesURL={#RepoUrl}/releases
#endif
VersionInfoVersion={#AppVersion}.0
VersionInfoDescription={#AppName} (Recomp) setup
PrivilegesRequired=lowest
DefaultDirName={autopf}\KameoRecomp
DisableProgramGroupPage=yes
; The welcome page says what the installer does not bring (Inno Setup 6 hides it by default).
DisableWelcomePage=no
UninstallDisplayName={#AppName} (Recomp)
UninstallDisplayIcon={app}\{#LauncherExe}
#if FileExists(AddBackslash(SourcePath) + "..\res\icon.ico")
SetupIconFile=..\res\icon.ico
#endif
WizardStyle=modern dark
WizardImageFile=wizard-202x386.png,wizard-336x643.png,wizard-430x824.png
WizardSmallImageFile=small-58.png,small-97.png,small-124.png
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; The launcher or the game running from the folder: closed with the Restart Manager before their files are replaced.
CloseApplications=yes
RestartApplications=no
Compression=lzma2/max
SolidCompression=yes
OutputDir={#OutputDir}
OutputBaseFilename=KameoRecomp-v{#AppVersion}-setup
ShowLanguageDialog=auto

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[Messages]
; The welcome page says what the installer does not bring: the game.
english.WelcomeLabel2=This will install [name/ver] on your computer.%n%nYou need your own copy of the game: the first time the launcher opens, it asks for the ISO of your Xbox 360 disc and extracts the game's files from it.
spanish.WelcomeLabel2=Este programa instalará [name/ver] en su sistema.%n%nNecesita su propia copia del juego: la primera vez que se abra, el launcher le pedirá la ISO de su disco de Xbox 360 y extraerá de ella los archivos del juego.
german.WelcomeLabel2=Dieser Assistent wird jetzt [name/ver] auf Ihrem Computer installieren.%n%nSie benötigen Ihre eigene Kopie des Spiels: Beim ersten Start fragt der Launcher nach der ISO Ihrer Xbox-360-Disc und extrahiert daraus die Spieldateien.
french.WelcomeLabel2=Cet assistant va vous guider dans l'installation de [name/ver] sur votre ordinateur.%n%nVous avez besoin de votre propre copie du jeu : à sa première ouverture, le launcher vous demandera l'ISO de votre disque Xbox 360 et en extraira les fichiers du jeu.
italian.WelcomeLabel2=[name/ver] sarà installato sul computer.%n%nServe la tua copia del gioco: la prima volta che si apre, il launcher ti chiederà la ISO del tuo disco Xbox 360 e ne estrarrà i file del gioco.

[CustomMessages]
english.DeleteUserData=Also delete your saves, achievements and settings?%n%nThey are in the game's folder (userdata and the .toml files). Choose No to keep them for a later installation.
spanish.DeleteUserData=¿Desea borrar también sus partidas guardadas, logros y ajustes?%n%nEstán en la carpeta del juego (userdata y los archivos .toml). Elija No para conservarlos para una instalación futura.
german.DeleteUserData=Sollen auch Ihre Spielstände, Erfolge und Einstellungen gelöscht werden?%n%nSie liegen im Ordner des Spiels (userdata und die .toml-Dateien). Wählen Sie Nein, um sie für eine spätere Installation zu behalten.
french.DeleteUserData=Supprimer aussi vos sauvegardes, succès et réglages ?%n%nIls se trouvent dans le dossier du jeu (userdata et les fichiers .toml). Choisissez Non pour les garder pour une installation ultérieure.
italian.DeleteUserData=Eliminare anche salvataggi, obiettivi e impostazioni?%n%nSi trovano nella cartella del gioco (userdata e i file .toml). Scegli No per conservarli per un'installazione futura.
english.DeleteGameFiles=Also delete the game files extracted from your disc (the assets folder, 5.4 GB)?%n%nTo play again, you would need your ISO.
spanish.DeleteGameFiles=¿Desea borrar también los archivos del juego extraídos de su disco (la carpeta assets, 5,4 GB)?%n%nPara volver a jugar necesitaría de nuevo su ISO.
german.DeleteGameFiles=Sollen auch die von Ihrer Disc extrahierten Spieldateien gelöscht werden (der Ordner assets, 5,4 GB)?%n%nUm wieder zu spielen, bräuchten Sie erneut Ihre ISO.
french.DeleteGameFiles=Supprimer aussi les fichiers du jeu extraits de votre disque (le dossier assets, 5,4 Go) ?%n%nPour rejouer, il vous faudrait à nouveau votre ISO.
italian.DeleteGameFiles=Eliminare anche i file del gioco estratti dal disco (la cartella assets, 5,4 GB)?%n%nPer giocare di nuovo servirà ancora la tua ISO.

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#ShortcutName}"; Filename: "{app}\{#LauncherExe}"
Name: "{autodesktop}\{#ShortcutName}"; Filename: "{app}\{#LauncherExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#LauncherExe}"; Description: "{cm:LaunchProgram,{#ShortcutName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; What the program writes itself besides the saves and the game files: an update's leftovers and the files a newer
; version added (the uninstaller only knows those it installed), the logs, the launcher's caches.
Type: filesandordirs; Name: "{app}\update"
Type: files; Name: "{app}\update-leftovers.txt"
Type: files; Name: "{app}\*.old-update"
Type: files; Name: "{app}\*.exe"
Type: files; Name: "{app}\*.dll"
Type: filesandordirs; Name: "{app}\licenses"
Type: filesandordirs; Name: "{app}\logs"

[Code]
var
  DeleteUserData: Boolean;
  DeleteGameFiles: Boolean;

// Before the files go, the questions; after, what was chosen. Nothing of the player's is deleted by a silent
// uninstall or by default (No is the default answer).
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  App: String;
begin
  App := ExpandConstant('{app}');
  if CurUninstallStep = usUninstall then
  begin
    DeleteUserData := False;
    DeleteGameFiles := False;
    if not UninstallSilent then
    begin
      if DirExists(App + '\userdata') or FileExists(App + '\kameo_options.toml') or FileExists(App + '\kameo.toml') then
        DeleteUserData := MsgBox(CustomMessage('DeleteUserData'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES;
      if FileExists(App + '\assets\default.xex') then
        DeleteGameFiles := MsgBox(CustomMessage('DeleteGameFiles'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES;
    end;
  end
  else if CurUninstallStep = usPostUninstall then
  begin
    if DeleteUserData then
    begin
      DelTree(App + '\userdata', True, True, True);
      DeleteFile(App + '\kameo.toml');
      DeleteFile(App + '\kameo_options.toml');
    end;
    if DeleteGameFiles then
      DelTree(App + '\assets', True, True, True);
    // The folder itself, once nothing is left in it.
    RemoveDir(App + '\assets');
    RemoveDir(App);
  end;
end;
