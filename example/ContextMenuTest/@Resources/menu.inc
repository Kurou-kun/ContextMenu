; Menu definition for the ContextMenu plugin.
;
; [Menu] is the background box + global text theme. Every other top-level
; section is a menu item; a section X\Y is a child of the item whose Submenu=X
; names the group. Box-style keys (Color, Fill, Image, Stroke, CornerRadius,
; Shadow, ShadowOffset, Padding) work in [Menu] and in any item section.

[Menu]
Fill=LinearGradient 90 ; 34,34,40,255 ; 20,20,26,255
Stroke=1,80,80,90,255
CornerRadius=8
Shadow=10,0,0,0,110
ShadowOffset=0,3
Padding=4
FontFace=Segoe UI
FontSize=10
TextColor=230,230,230,255
HoverBgColor=70,70,110,255
ItemHeight=28

[Refresh]
Text=Refresh skin
Bang=!Refresh

[Edit]
Text=Edit skin
Bang=!EditSkin

[Sep1]
Separator=1
Height=3
Color=120,110,150,255
CornerRadius=2
Padding=16,4,16,4

[Accent]
Text=Accent item
Bang=!About
Color=60,60,90,255
CornerRadius=6

[Tools]
Text=Tools
Submenu=Tools
[Tools\About]
Text=About Rainmeter
Bang=!About
[Tools\More]
Text=More
Submenu=Tools\More
[Tools\More\Deeper]
Text=Deeper
Submenu=Tools\More\Deeper
[Tools\More\Deeper\Log]
Text=Level 4 - Show log
Bang=!About
