; ContextMenu v2 menu definition.
;
; [Menu] = background box + global text/layout defaults.
; Any other top-level section is an item; a section X\Y is a child of the item
; whose Submenu=X names the group. A section whose name starts with "Separator"
; is a separator row. A single section named [Title] is a non-interactive header
; (Text defaults to the skin config path). Box/text keys work in [Menu] and any
; item/title/separator.

[Menu]
BgColor=30,30,30,255
Gradient=Linear 90 ; 34,34,40,255 ; 20,20,26,255
StrokeWidth=1
StrokeColor=80,80,90,255
CornerRadius=8
Shadow=10 ; 0,0,0,110
ShadowOffset=0,3
Padding=4
FontFace=Segoe UI
FontSize=10
FontColor=230,230,230,255
BgHoverColor=70,70,110,255
ItemHeight=28
MaxWidth=220

[Title]
; Text omitted => shows the skin config path (#CURRENTCONFIG#)
FontColor=150,150,170,255
FontAlign=center
FontSize=9

[Refresh]
Text=Refresh skin
Action=!Refresh
Icon=%system%\imageres.dll,228

[Edit]
Text=Edit skin
Action=!EditSkin

[Separator_1]
Color=120,110,150,255
Height=2
CornerRadius=1
Padding=16,4,16,4

[Accent]
Text=Accent item
Action=!About
BgColor=60,60,90,255
BgHoverColor=90,90,140,255
FontAlign=center
FontCase=Upper
CornerRadius=6

[Tools]
Text=Tools
Submenu=Tools
[Tools\About]
Text=About Rainmeter
Action=!About
IconPos=Right
Icon=%system%\imageres.dll,76
[Tools\Quiet]
Text=Quiet item
Action=!About
SubmenuIco=0
[Tools\Separator_A]
Color=90,90,110,255
Height=1
[Tools\Disabled]
Text=Unavailable
Disabled=1
