from pathlib import Path
root=Path('build/path-fixtures')
for name in ['data/etc/message/option_msg_j.mes','sh2e/etc/message/option_msg_j.mes','lang/etc/message/option_msg_j.mes',
             'data/etc/message/common_msg_j.mes','sh2e/etc/message/common_msg_j.mes','data/etc/message/item_msg_j.mes',
             'lang/etc/resource/r_menu_e.res','lang/etc/resource/r_menu_j.res','lang/pic/etc/start01j.tex','data/save/test.dat']:
    p=root/name; p.parent.mkdir(parents=True,exist_ok=True); p.write_text(name)
(root/'sh2e.ini').write_text('etc=1\npic=1\n')
