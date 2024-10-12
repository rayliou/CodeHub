all:smbus_i2c
	./$<
	./b.py
install:
	sudo docker run -it --rm   --device /dev/i2c-7 \
		-v ./:/app   -w /app   alpine:latest sh \
		-c "apk update && apk add make gcc g++ i2c-tools build-base linux-headers bash && bash"
.PHONY:clean
clean:
	rm -vf smbus_i2c

