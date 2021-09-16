.segment "RODATA"

.export _title_nametable
.export _main_nametable

_title_nametable: .incbin "nametables/title.rle"
_main_nametable: .incbin "nametables/main.rle"
