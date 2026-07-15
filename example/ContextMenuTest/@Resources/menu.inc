; ContextMenu v2 menu definition.
;
; [Menu] = background box + global text/layout defaults.
; Any other top-level section is an item; a section X\Y is a child of the item
; whose Submenu=X names the group. A section whose name starts with "Separator"
; is a separator row. A single section named [Title] is a non-interactive header
; (Text defaults to the skin config path). Box/text keys work in [Menu] and any
; item/title/separator.
;
; @Include pulls in another file (paths support #@#, #CURRENTPATH#, etc.); its
; [Variables] feed #token# substitution below. AntiAlias is off by default
; (crisp) per section; set it where you want smoothed shapes/text. Padding is 0
; by default everywhere — you own every inset.

@Include=#@#theme.inc

[Menu]
BgColor=#bg#
Gradient=Linear 90 ; 34,34,40,255 ; 20,20,26,255
StrokeWidth=1
StrokeColor=80,80,90,255
CornerRadius=8
Shadow=10 ; 0,0,0,110
ShadowOffset=0,3
Padding=4
AntiAlias=1
AnimFade=1
FontFace=Segoe UI
FontSize=10
FontColor=230,230,230,255
BgHoverColor=70,70,110,255
ItemHeight=28
MaxWidth=220

[Title]
; Text omitted => shows the skin config path (#CURRENTCONFIG#)
FontColor=#dim#
FontAlign=center
FontSize=9
FontWeight=Bold

[Refresh]
Text=Refresh skin
Action=!Refresh
Icon=%system%\imageres.dll,228
Padding=8,0,8,0

[Edit]
Text=Edit skin
Action=[!EditSkin][!Log "ContextMenu: edit fired"]
Padding=8,0,8,0

[Separator_1]
Color=#accent#
Height=2
CornerRadius=1

[Accent]
Text=Accent item
Action=!About
BgColor=#accent#
BgHoverColor=#accentHover#
FontAlign=center
FontCase=Upper
FontWeight=Bold
CornerRadius=6
AntiAlias=1
Padding=8,0,8,0

[Tools]
Text=Tools
Submenu=Tools
Padding=8,0,8,0
[Tools\About]
Text=About Rainmeter
Action=!About
IconPos=Right
Icon=%system%\imageres.dll,76
Padding=8,0,8,0
[Tools\Quiet]
Text=Quiet item
Action=!About
Chevron=0
Padding=8,0,8,0
[Tools\Separator_A]
Color=90,90,110,255
Height=1
[Tools\Disabled]
Text=Unavailable
Disabled=1
Icon=%system%\imageres.dll,76
Padding=8,0,8,0
