import xlwt

book = xlwt.Workbook(encoding="utf-8")
sheet1 = book.add_sheet("Data Sheet")

cols = ["A", "B", "C", "D", "E"]
f = open("data.txt", 'r')
data = []


for line in f:
    pos = line.find('-')
    if pos != -1:
        pos1 = line.find('<', pos)

        if pos1 == -1:
            data.append((line[0:pos-1], line[pos+2]))
        else:
            first_num_pos = line.find('>', pos)
            first_num = line[pos1+1: line.find('>', first_num_pos)]

            sec_num_st = line.find('<', first_num_pos)
            sec_num_en = line.find('>', sec_num_st)
            sec_num = line[sec_num_st+1: sec_num_en]

            pac_name_st = line.find('<', sec_num_en)
            pac_name_en = line.find('>', pac_name_st)
            pac_name = line[pac_name_st+1: pac_name_en]

            source_name_st = line.find('<', pac_name_en)
            source_name_en = line.find('>', source_name_st)
            source_name = line[source_name_st + 1: source_name_en]


            data.append((first_num, sec_num, pac_name, source_name))

print(data)

for num in range(len(data)):
    row = sheet1.row(num)
    ofs = 0
    value = data[num]
    if len(value) != 2:
        ofs = 0 if value[3] == '0' else 4

    for col in range(len(value)):
        row.write(col + ofs, str(value[col]))


book.save('result.xls')
