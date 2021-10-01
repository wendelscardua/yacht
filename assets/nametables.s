.segment "RODATA"

.export _title_nametable
.export _main_nametable
.export _help1_nametable
.export _help2_nametable

_title_nametable: .incbin "nametables/title.rle"
_main_nametable: .incbin "nametables/main.rle"
_help1_nametable: .incbin "nametables/help1.rle"
_help2_nametable: .incbin "nametables/help2.rle"
