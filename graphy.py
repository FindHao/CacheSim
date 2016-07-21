import re
import matplotlib.pyplot as plt
mapping_ways = [1, 2, 4, 8, 12, 16]
line_size = [32, 64, 128]
swap_style = [0, 1, 2]
class rate:
	def __init__(self, size, way, swap):
		self.way = int(way)
		self.size = int(size)
		self.swap = int(swap)
		self.miss_rate = 0
		self.read = 0
		self.write = 0
	def __str__(self):
		return "%s %s" %(self.way,self.size)

rates = []
def work():
	global rates
	with open('input', 'r')as f:
		line = f.readline()
		while line:
			if line.find('line_size') >=0:
				reg = re.compile("line_size: (\d+).+mapping ways (\d+).+swap_style (\d+)")
				ans = reg.findall(line)
				ans = ans[0]
				arate = rate(ans[0], ans[1],ans[2])
				f.readline()
				f.readline()
				line = f.readline()
				reg2 = re.compile(".+hit rate.+/([\d|\.]+)")
				ans2 = reg2.findall(line)
				ans2 = ans2[0]
				arate.miss_rate = float(ans2)
				line = f.readline()
				reg3 = re.compile(".+ (\d+)KB")
				ans3 = reg3.findall(line)[0]
				print(ans3)
				arate.read = int(ans3)
				line = f.readline()
				ans4 = reg3.findall(line)[0]
				arate.write = int(ans4)
				rates.append(arate)
			line = f.readline()

def draw():
	global rates
	# 先画相同替换策略 0 ，相同cache line size 32kb，不同way的miss rate
	# y  = []
	# x = []
	# for node in rates:
	# 	# print(type(node.swap), type(node.size))
	# 	if	(node.size == 32) and (node.swap == 0):
	# 		y.append(node.miss_rate)
	# 		x.append(node.way)
	# print(x)
	# print(y)
	# plt.plot(x, y, 'r')

	# 不同替换策略
	x1 = []
	y1 = []
	y2 = []
	for node in rates:
		if node.size == 32 and node.way == 8:
			x1.append(node.swap)
			# y1.append(node.miss_rate)
			y1.append(node.read)
			y2.append(node.write)
	print(x1, y1)
	plt.bar(x1, y1, alpha=.5, color = 'g')
	plt.bar(x1, y2, alpha=.5, color = 'r')


	plt.show()

work()
draw()