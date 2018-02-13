bmp_traceswo: bmp_traceswo.cpp
	gcc -g -o $@ $< -lusb-1.0 -lstdc++
