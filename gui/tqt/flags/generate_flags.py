import os
import glob

os.chdir(os.path.dirname(os.path.abspath(__file__)))

flags = [f for f in glob.glob("*.png")]
flags.sort()

# Read ISO codes mapping code -> name
iso_map = {}
try:
    with open('/usr/share/zoneinfo/iso3166.tab', 'r') as f:
        for line in f:
            if line.startswith('#'): continue
            parts = line.strip().split('\t', 1)
            if len(parts) == 2:
                # e.g iso_map["fr"] = "france"
                iso_map[parts[0].lower()] = parts[1].lower()
except Exception as e:
    print(e)
    pass

# NordVPN specific overrides or names
iso_map["us"] = "united states"
iso_map["gb"] = "united kingdom"
iso_map["kr"] = "south korea"
iso_map["mk"] = "north macedonia"
iso_map["ae"] = "united arab emirates"
iso_map["tw"] = "taiwan"
iso_map["ru"] = "russia"
iso_map["vn"] = "vietnam"
iso_map["tr"] = "turkey"
iso_map["sy"] = "syria"
iso_map["md"] = "moldova"
iso_map["ba"] = "bosnia and herzegovina"
iso_map["bn"] = "brunei darussalam"
iso_map["la"] = "lao peoples democratic republic"
iso_map["ly"] = "libyan arab jamahiriya"
iso_map["mm"] = "myanmar"
iso_map["tt"] = "trinidad and tobago"
iso_map["sv"] = "el salvador"
iso_map["cr"] = "costa rica"

header_path = "../src/flags_icons.h"

with open(header_path, "w") as out:
    out.write("#pragma once\n")
    out.write("#include <tqmap.h>\n")
    out.write("#include <tqstring.h>\n\n")
    out.write("struct FlagIconData {\n")
    out.write("    const unsigned char* data;\n")
    out.write("    unsigned int len;\n")
    out.write("};\n\n")

for f in flags:
    os.system(f"xxd -i '{f}' >> {header_path}")

with open(header_path, "a") as out:
    out.write("\nstatic TQMap<TQString, FlagIconData> getFlagsMap() {\n")
    out.write("    TQMap<TQString, FlagIconData> map;\n")
    for f in flags:
        code = f.replace(".png", "").lower()
        var_name = f.replace(".", "_").replace("-", "_").lower()
        
        # Default iso code
        out.write(f"    map.insert(\"{code}\", {{ {var_name}, {var_name}_len }});\n")
        
        # Country name
        country_name = iso_map.get(code)
        if country_name:
            # e.g "france" -> fr_png
            out.write(f"    map.insert(\"{country_name}\", {{ {var_name}, {var_name}_len }});\n")
            
    out.write("    return map;\n")
    out.write("}\n")
