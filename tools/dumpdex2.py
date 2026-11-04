from loguru import logger
logger.remove()
from androguard.core.dex import DEX
d = DEX(open(r'H:\GAMES\FAST5-NX\game\apk_extracted\classes.dex','rb').read())
for c in d.get_classes():
    n = c.get_name()
    if 'GAND/GloftF5HP/ML/' not in n:
        continue
    for m in c.get_methods():
        code = m.get_code()
        if not code:
            continue
        L = ["    %s %s" % (i.get_name(), i.get_output()) for i in code.get_bc().get_instructions()]
                if any(k in l for l in L for k in ('GLGame;->c I', 'InitDeviceValues')):
            print(n, m.get_name(), m.get_descriptor()); print("\n".join(L))