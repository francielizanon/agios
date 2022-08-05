import pandas as pd
from numpy import array, linspace
import matplotlib.pyplot as plt
import numpy as np





# Load all the csv files to dataframes in l1
df = pd.read_csv("wfq/output.csv")



## Generate the Brandwidth Graph
fig, ax = plt.subplots()

fig.set_size_inches(11, 8.5, forward=True)
# Plot
# plt.scatter(x_fileSize, df.mean().values, alpha = .6, color='lightblue', label = '')
#ax.plot(df.index.values, df.set_4.values,'--', label='Set 04')
#ax.plot(df.index.values, df.set_3.values,'--', label='Set 03')
ax.plot(df.index.values, df.set_2.values,'--', label='Set 02')
ax.plot(df.index.values, df.set_1.values,'--', label='Set 01')

#ax.fill_between(x_labels, y_max, y_min, color='blue', alpha=0.1, label='Max/min interval')

# Define Labels
#plt.xticks(rotation=45)
plt.xlabel('Timestamp', fontweight='bold',  fontsize=14)
plt.ylabel('Bandwidth Proportion', fontsize=14,fontweight='bold')
# plt.title('Bandwidth evolution according to the transferred aggregated file size')
# plt.ylim([500, 5000])

#ax.tick_params(axis='both', which='major', labelsize=14)

plt.legend()
plt.grid()



fig.savefig('wfq_graph03.pdf', format='pdf', dpi=1200)
plt.show()
