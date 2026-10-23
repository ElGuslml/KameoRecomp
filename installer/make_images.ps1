# Renders the installer's wizard images from this project's own art with a headless Chrome (or Edge):
#   - wizard-<w>x<h>.png from launcher\art\install.svg (the launcher's first-run art), for the Welcome and Finished
#     pages' image area (164:314; 100 %, 150 % and 200 % of Inno Setup 6.6's sizes);
#   - small-<n>.png from res\icon.svg (the icon) when there is one, otherwise from the same art, for the inner pages'
#     top right (58, 97 and 124 px).
# The PNGs are committed; run this again after changing the art or the icon. POTCAWERecomp's, made general.
#
#   .\installer\make_images.ps1
$ErrorActionPreference = "Stop"
$repo = (Resolve-Path "$PSScriptRoot\..").Path
$browser = @("$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
             "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe") | Where-Object { Test-Path $_ } |
  Select-Object -First 1
if (-not $browser) { throw "Chrome or Edge is needed to render the images" }
$work = Join-Path $env:TEMP "$(Split-Path $repo -Leaf)-installer-images"
New-Item -ItemType Directory -Force $work | Out-Null

function Render([string]$svg, [int]$width, [int]$height, [string]$fit, [string]$out) {
  $content = [System.IO.File]::ReadAllText($svg)
  # The art fills the area as the launcher draws it (preserveAspectRatio), the icon is centered.
  $content = $content -replace '<svg ', "<svg preserveAspectRatio=""$fit"" width=""$width"" height=""$height"" "
  $content = $content -replace ' width="256" height="256"', ''
  $html = "<!doctype html><html><head><style>html,body{margin:0;padding:0;overflow:hidden;background:transparent}" +
          "svg{display:block}</style></head><body>$content</body></html>"
  $page = Join-Path $work "page.html"
  [System.IO.File]::WriteAllText($page, $html)
  $shot = Join-Path $work "shot.png"
  if (Test-Path $shot) { Remove-Item $shot }
  # The browser writes harmless messages to its error output: started as a process, they stay out of the way.
  Start-Process -FilePath $browser -Wait -WindowStyle Hidden -RedirectStandardError (Join-Path $work "browser.log") `
    -ArgumentList "--headless=new", "--disable-gpu", "--hide-scrollbars", "--default-background-color=00000000",
                  "--window-size=$width,$height", "--screenshot=$shot", "file:///$($page -replace '\\', '/')"
  for ($i = 0; $i -lt 50 -and -not (Test-Path $shot); $i++) { Start-Sleep -Milliseconds 100 }
  Copy-Item $shot (Join-Path $PSScriptRoot $out) -Force
  "$out ${width}x${height}"
}

foreach ($size in @(@(202, 386), @(336, 643), @(430, 824))) {
  Render "$repo\launcher\art\install.svg" $size[0] $size[1] "xMidYMax slice" "wizard-$($size[0])x$($size[1]).png"
}
$icon = "$repo\res\icon.svg"
foreach ($size in @(58, 97, 124)) {
  if (Test-Path $icon) {
    Render $icon $size $size "xMidYMid meet" "small-$size.png"
  } else {
    Render "$repo\launcher\art\install.svg" $size $size "xMidYMid slice" "small-$size.png"
  }
}
