


path = "C:/Users/seanf/Desktop/BurstSketch-master/BurstSketch-master/High-speed/sx-stackoverflow.txt"


with open(path, 'r') as f:
    lines = f.readlines()

sampled = lines[0:3000000]

with open('sampled_dataset.txt', 'w', newline='') as f:  
    f.writelines(sampled)
