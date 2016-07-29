#!/usr/bin/python3
import sys
import re
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import numpy as np

mapping_ways = [1, 2, 4, 8, 12, 16]
line_size = [32, 64, 128]
swap_style = [0, 1, 2]
class rate:
	def __init__(self, cache_size, line_size, way, swap):
		self.way = int(way)
		self.line_size = int(line_size)
		self.size = int(cache_size)
		self.swap = int(swap)
		self.miss_rate = 0
		self.read = 0
		self.write = 0

# 没有cache locking的和支持的。
rates = []
rates_cache_locking = []


def read_input(a_rates, input_filename):
	"""从不同的输入文件里，获得不同的统计结果
	"""
	with open(input_filename, 'r')as f:
		line = f.readline()
		while line:
			if line.find('line_size') >=0:
				reg = re.compile("cache_size: (\d+) Bytes.+line_size: (\d+).+mapping ways (\d+).+swap_style (\d+)")
				ans = reg.findall(line)
				ans = ans[0]
				arate = rate(ans[0], ans[1],ans[2],ans[3])
				f.readline()
				f.readline()
				line = f.readline()
				reg2 = re.compile(".+hit rate.+/([\d|\.]+)")
				ans2 = reg2.findall(line)
				ans2 = ans2[0]
				arate.miss_rate = float(ans2)
				line = f.readline()
				reg3 = re.compile(".+ (\d+) Bytes")
				ans3 = reg3.findall(line)[0]
				arate.read = int(ans3)
				line = f.readline()
				ans4 = reg3.findall(line)[0]
				arate.write = int(ans4)
				a_rates.append(arate)
			line = f.readline()
	return a_rates


def draw_comparation():
	y = []
	y2 = []
	x = np.arange(3)
	bar_width = 0.5
	for node in rates:
		if node.line_size == 32 and node.way == 8 and node.size == 131072:
			y.append(node.miss_rate)
	# cache locking data
	for node in rates_cache_locking:
		if node.line_size == 32 and node.way == 8 and node.size == 131072:
			y2.append(node.miss_rate)

	plt.bar(x, y ,width=bar_width, alpha=.5, color='g')
	plt.bar(x, y2 ,width=bar_width, alpha=.5, color='r')
	plt.show()

rates = read_input(rates ,'input')
draw_comparation()
