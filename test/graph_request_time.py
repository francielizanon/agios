import pandas as pd
from numpy import array, linspace
import matplotlib.pyplot as plt
import numpy as np





# Load all the csv files to dataframes in l1
df = pd.read_csv("../build/timestamp_output.csv")



## Generate the Brandwidth Graph
fig, ax = plt.subplots()

fig.set_size_inches(11, 8.5, forward=True)
# Plot
# plt.scatter(x_fileSize, df.mean().values, alpha = .6, color='lightblue', label = '')
#ax.plot(df.groupby("queue_id").get_group(0).start_time, df.groupby("queue_id").get_group(0).elapsed,'--', label='Queue 0 (500)')
#ax.plot(df.groupby("queue_id").get_group(1).start_time, df.groupby("queue_id").get_group(1).elapsed,'--', label='Queue 1 (1000)')
#ax.plot(df.groupby("queue_id").get_group(2).start_time, df.groupby("queue_id").get_group(2).elapsed,'--', label='Queue 2 (1500)')
#ax.plot(df.groupby("queue_id").get_group(3).start_time, df.groupby("queue_id").get_group(3).elapsed,'--', label='Queue 3 (2000)')
ax.plot(df.start_time, df.elapsed,'--', label='TO')

#ax.fill_between(x_labels, y_max, y_min, color='blue', alpha=0.1, label='Max/min interval')

# Define Labels
#plt.xticks(rotation=45)
plt.xlabel('Start Time (ns)', fontweight='bold',  fontsize=14)
plt.ylabel('Elapsed Time (ns)', fontsize=14,fontweight='bold')
# plt.title('Bandwidth evolution according to the transferred aggregated file size')
# plt.ylim([500, 5000])

#ax.tick_params(axis='both', which='major', labelsize=14)

plt.legend()
plt.grid()



fig.savefig('graph_request_time.pdf', format='pdf', dpi=1200)
plt.show()
