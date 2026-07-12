; Menu definition for the ContextMenu plugin.
;
; [Menu] is the theme section (all keys optional; defaults shown are a dark
; theme). Every other top-level section is a menu item, rendered in file order.
; A section named X\Y is a child of the item whose Submenu=X names the group.

[Menu]
BgColor=30,30,30,255
TextColor=230,230,230,255
HoverBgColor=60,60,90,255
SeparatorColor=80,80,80,255
FontFace=Segoe UI
FontSize=10
ItemHeight=28
CornerRadius=8

[Refresh]
Text=Refresh skin
Bang=!Refresh
; Icon accepts .png / .ico / .bmp; #@# is the skin's @Resources folder:
; Icon=#@#refresh.png

[Edit]
Text=Edit skin
Bang=!EditSkin

[Sep1]
Separator=1

[Disabled]
Text=Unavailable
Disabled=1

[Tools]
Text=Tools
Submenu=Tools
[Tools\About]
Text=About Rainmeter
Bang=!About
[Tools\Manage]
Text=Manage
Bang=!Manage
