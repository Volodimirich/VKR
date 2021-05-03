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
            first_num = line[pos1+1: line.find('>', pos)]
            sec_num = line[line.rfind('<')+1: line.rfind('>')]
            data.append((first_num, sec_num))

cols = ["A", "B", "C"]
print(data)

for num in range(len(data)):
    row = sheet1.row(num)
    value = data[num]
    row.write(0, str(value[0]))
    row.write(1, str(value[1]))


book.save('result.xls')
