def generate_header():
    with open('tmp.bin', 'rb') as f:
        data = f.read()

    with open('image_data.h', 'w') as f:
        f.write(f"const uint16_t image_data[] PROGMEM = {{\n")
        for i in range(0, len(data), 2):
            value = int.from_bytes(data[i:i+2], 'big')
            f.write(f"0x{value:04X}, ")
            if (i // 2 + 1) % 10 == 0:
                f.write("\n")
        f.write("};")

    os.remove("tmp.bin")

def convert_to_rgb565(image_path):
    image = Image.open(image_path).convert('RGB')
    rgb565_data = []
    for pixel in image.getdata():
        r, g, b = pixel
        # Convert RGB888 to RGB565, if image show up in blue or red, swap 'r' and 'b' here
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        # Swap bytes for little-endian format, not used
        rgb565_swapped = ((rgb565 & 0xFF) << 8) | (rgb565 >> 8)
        # if image is not showing normally, change rgb565 to rgb565_swapped
        rgb565_data.append(rgb565)

    with open('tmp.bin', 'wb') as f:
        for value in rgb565_data:
            f.write(value.to_bytes(2, 'big'))

def main(args):
    convert_to_rgb565(args[1])
    generate_header()
    print("convert complete.")

if __name__ == "__main__":
    import os
    import sys
    from PIL import Image
    if len(sys.argv) != 2:
        print("usage: python3 image2header_cvt.py <source_file_name>")
        exit()
    main(sys.argv)