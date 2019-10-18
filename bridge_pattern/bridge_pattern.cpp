#include <iostream>
#include <string>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char U8;
typedef signed long S32;
typedef unsigned long U32;
typedef signed int S16;
typedef unsigned int U16;
typedef unsigned int LD16;


class
CompressAPI {
    public:
	virtual void Compress(unsigned char *OutData, unsigned char *InData, int decompressedSize, int *out_size) const = 0;
	virtual void Decompress(unsigned char *OutData, unsigned char *InData, int decompressedSize, int *out_size) const = 0;
};


class
CompressAPI_RLE : public CompressAPI {
    public:
	void Compress(unsigned char *OutData, unsigned char *InData, int decompressedSize, int *out_size) const override {
		int InSeries, Repetitive;
		unsigned char *CurByte, *PreviousByte, *Series;
		unsigned long int Pos;

		Pos = 0;
		do {
			*PreviousByte = *CurByte;
			*CurByte = *(InData++);

			Pos++;

			if (*PreviousByte != *CurByte) {
				if ((InSeries == 1) && (Repetitive == 1)) {
					if (strlen((const char *) Series) > 1) {
						*OutData += (unsigned char) strlen((const char *) Series);
						strncat((char *) OutData, (const char *) Series, 1);
						*Series = CurByte[0];
						Repetitive = 0;
					} else {
						if (strlen((const char *) Series) < 255) {
							strcat((char *) Series, (const char *) CurByte);
							Repetitive = 0;
						} else {
							*OutData += (unsigned char) strlen((const char *) Series);
							strncat((char *) OutData, (const char *) Series, 1);
							OutData += 2;
							*Series = CurByte[0];
							Repetitive = 0;
						}
					}

					if (Pos >= strlen((const char *) InData))
						InSeries = 0;
				} else if ((InSeries == 1) && (Repetitive == 0)) {
					if (strlen((const char *) Series) < 255) {
						strcat((char *) Series, (const char *) CurByte);
						Series++;
					} else {
						*(OutData++) = '\0';
						*(OutData++) = (unsigned char) strlen((const char *) Series);
						strcat((char *) OutData, (const char *) Series);
						if ((strlen((const char *) Series) % 2) != 0)
							*(OutData++) = '\0';
						*Series = CurByte[0];
					}
				} else {
					*Series = CurByte[0];
					InSeries = 1;
					Repetitive = 0;
				}
			} else {
				if ((InSeries == 1) && (Repetitive == 0)) {
					if (strlen((const char *) Series) > 1) {
						*(OutData++) = '\0';
						*(OutData++) = (unsigned char) strlen((const char *) Series);
						strcat((char *) OutData, (const char *) Series);
						if ((strlen((const char *) Series) % 2) != 0)
							*(OutData++) = '\0';
						*Series = CurByte[0];
						Repetitive = 1;
					} else {
						if (strlen((const char *) Series) < 255) {
							strcat((char *) Series, (const char *) CurByte);
							Repetitive = 1;
						} else {
							*(OutData++) = '\0';
							*(OutData++) = (unsigned char) strlen((const char *) Series);
							strcat((char *) OutData, (const char *) Series);
							if ((strlen((const char *) Series) % 2) != 0)
								*(OutData++) = '\0';
							*Series = CurByte[0];
							Repetitive = 1;
						}
					}

					if (Pos >= strlen((const char *) InData))
						InSeries = 0;
				} else if ((InSeries == 1) && (Repetitive == 1)) {
					if (strlen((const char *) Series) < 255) {
						strcat((char *) Series, (const char *) CurByte);
						Series++;
					} else {
						*(OutData++) = (unsigned char) strlen((const char *) Series);
						strncat((char *) OutData, (const char *) Series, 1);
						*Series = CurByte[0];
					}
				} else {
					*Series = CurByte[0];
					InSeries = 1;
					Repetitive = 1;
				}
			}

			if (Pos >= strlen((const char *) InData))  break;
		} while (Pos < strlen((const char *) InData));

		if ((InSeries == 1) && (Repetitive == 1)) {
			*(OutData++) = (unsigned char) strlen((const char *) Series);
			strncat((char *) OutData, (const char *) Series, 1);
		} else if ((InSeries == 1) && (Repetitive == 0)) {
			*(OutData++) = '\0';
			*(OutData++) = (unsigned char) strlen((const char *) Series);
			strcat((char *) OutData, (const char *) Series);
			if ((strlen((const char *) Series) % 2) != 0)
				*(OutData++) = '\0';
		}

		*(OutData++) = '\0';
		*(OutData++) = '\1';

		*out_size = Pos;
	}

	void Decompress(unsigned char *OutData, unsigned char *InData, int decompressedSize, int *out_size) const override {
		unsigned char *CurByte, *NextByte, *Series, *Buff, *Count;
		unsigned long int Pos;

		Pos = 0;
		do {
			*CurByte = *(InData++);
			Pos++;

			if ((int) CurByte > 0) {
				*NextByte = *(InData++);
				memset(OutData, NextByte[0], (int) CurByte);
				OutData += (int) CurByte;

				Pos++;
			} else {
				*Count = *(InData++);
				Pos++;

				if ((int) Count[0] > 1) {
					strncat((char *) OutData, (const char *) InData, (int) Count);

					if (((int) Count % 2) != 0)
						Pos++;
				} else if ((int) Count[0] == 1)
					break;
			}

			if (Pos >= strlen((const char *) InData))
				break;
		} while (Pos < strlen((const char *) InData));

		*out_size = Pos;
	}
};


class
CompressAPI_LZMIT : public CompressAPI {
    private:
	int GetMaxSize(unsigned char *source_search, unsigned char *source) const {
		int r = 0;
        	while ((*source_search == *source) && (r < 18)) {
			source_search++;
			source++;
			r++;
		}
		return r;
	}

	bool CompressBytes(unsigned char *source, unsigned short int *pos, int source_back) const {
		if (source_back > 4096)
			source_back = 4096;

		unsigned char *source_search = (source - source_back);
		int max_size = 3;
		int back_pos = source_back;
		bool r = false;
		int t, bp2;

		while (back_pos > 0) {
			t = GetMaxSize(source_search, source);
			if (t >= max_size) {
				if (t > max_size)
					bp2 = back_pos - 1;

				max_size = t;

				(*pos) = (bp2);
				(*pos) <<= 4;
				(*pos) |= (t - 2 - 1);

				r = true;
			}

			/* Modified by OBrasilo: imported from Zink's code */
			if (t >= 18)
				break;

			source_search++;
			back_pos--;
		}

		return r;
	}

    public:
	void Compress(unsigned char *OutData, unsigned char *InData, int decompressedSize, int *out_size) const override {
		unsigned char *indic_last;
		unsigned short int indic_bits_left = 0;
		unsigned short int pos;
		int decompressedSize_unchanged = decompressedSize;

		/* Set output size */
		*out_size = 0;
		do {
			if (indic_bits_left == 0) {
				/* Reserve space for indicator */
				indic_last = OutData++;
				(*indic_last) = 0x00;
				indic_bits_left = 8;
				/* Increase output size by one */
				(*out_size)++;
			}

			if (CompressAPI_LZMIT::CompressBytes(InData, &pos, (decompressedSize_unchanged - decompressedSize))) {
			/* Bytes can be compressed */
	                        InData += (pos & 0x0F) + 3;
        	                decompressedSize -= (pos & 0x0F) + 3;
				(*out_size) += 2;
				/* Write bytes to destination */
				*(OutData++) = (pos & 0xFF);
				*(OutData++) = (pos >> 8);
			} else {
				/* Cannot compress bytes */
				*(OutData++) = *(InData++);
				decompressedSize--;
				/* Increase output size by one */
				(*out_size)++;
				/* Set indicator bit to 1 */
				(*indic_last) |= 0x01;
			}

			/* Calculate indicator bits left */
			indic_bits_left--;
			/* If the bit we are processing is not the last, then
			   shift so there is space for more bits */
			if (indic_bits_left > 0)
				(*indic_last) <<= 1;

			if (indic_bits_left == 0) {
				/* Reverse bits of indicator */
				unsigned char h = (*indic_last);
				unsigned char r = 0;
				if (h & 128) r |= 1;
				if (h & 64) r |= 2;
				if (h & 32) r |= 4;
				if (h & 16) r |= 8;
				if (h & 8) r |= 16;
				if (h & 4) r |= 32;
				if (h & 2) r |= 64;
				if (h & 1) r |= 128;
				(*indic_last) = r;
			}
			/* LOOP while there are input data */
		} while (decompressedSize);

		(*indic_last) <<= indic_bits_left-1;

		if (indic_bits_left > 0) {
		/* Reverse bits of indicator */
			unsigned char h = (*indic_last);
			unsigned char r = 0;
			if (h & 128) r |= 1;
			if (h & 64) r |= 2;
			if (h & 32) r |= 4;
			if (h & 16) r |= 8;
			if (h & 8) r |= 16;
			if (h & 4) r |= 32;
			if (h & 2) r |= 64;
			if (h & 1) r |= 128;
			(*indic_last) = r;
		}
	}

	void Decompress(unsigned char *OutData, unsigned char *InData, int decompressedSize, int *out_size) const override {
		*out_size = 0;

		while( decompressedSize > 0 ) {
			U8 bits;
			U8 type = *InData++;

			for(bits = 1; bits != 0; bits <<= 1) {
				if (type & bits) {
					*OutData++ = *InData++;
					*out_size++;
					decompressedSize--;
				} else {
					S32 offset = *(U16 *) InData;
					S32 length = (offset & 0xF) + 3;
					U8 *ptr;

					InData += 2;
					offset >>= 4;	/* Rotate by 4 bits */

					ptr = OutData - offset - 1;

					if (offset == 0) {
						/* Fill with byte */
						memset(OutData, *ptr, length);
					} else {
						/* Copy block */
						if ((ptr + length) >= OutData) {
							/* Correct overlap bug */
							S32 n;
							U8 *tmp = OutData;
							for(n = length; n > 0; n--)
								*tmp++ = *ptr++;
						} else
							memcpy(OutData, ptr, length);
					}

					decompressedSize -= length;
					OutData += length;
					*out_size += length;
				}

				if (decompressedSize <= 0)
					return;
			}
		}
	}
};


class FileHandler {
    public:
	FileHandler(char *lpExt, const CompressAPI& compress_api) : compress_api_(compress_api) {
		ext = lpExt;
	}

	virtual void Compress() {
		compress_api_.Compress(OutBuf, InBuf, InSize, &OutSize);
	}

	virtual void Decompress() {
		compress_api_.Decompress(OutBuf, InBuf, InSize, &OutSize);
	}

	virtual int Read(char *path) {
		f = fopen(path, "rb");
		if (f == NULL)
			return 0;
		fseek(f, 0, SEEK_END);
		InSize = ftell(f);
		if (InSize == 0) {
			fclose(f);
			f = NULL;
			return 0;
		}
		InBuf = (unsigned char *) malloc(InSize);
		if (InBuf == 0) {
			fclose(f);
			f = NULL;
			return 0;
		}
		OutBuf = (unsigned char *) malloc(InSize << 1);
		if (OutBuf == 0) {
			free(InBuf);
			InBuf = NULL;
			fclose(f);
			f = NULL;
			return 0;
		}
		fseek(f, 0, SEEK_SET);
		fread(InBuf, 1, InSize, f);
		OutSize = 0;
		return 1;
	}

	virtual int Write(char *path) {
		g = fopen(path, "wb");
		if (g == NULL)
			return 0;
		fseek(f, 0, SEEK_SET);
		fwrite(OutBuf, 1, OutSize, f);
		return 1;
	}

	virtual char *GetExt(void) {
		return ext;
	}

	virtual void Close(void) {
		if (OutBuf != NULL) {
			free(OutBuf);
			OutBuf = NULL;
		}
		if (InBuf != NULL) {
			free(InBuf);
			InBuf = NULL;
		}
		if (f != NULL) {
			fclose(f);
			f = NULL;
		}
		if (g != NULL) {
			fclose(g);
			g = NULL;
		}
	}

    private:
  	char *ext;
	FILE *f, *g;
	unsigned char *InBuf, *OutBuf;
	int InSize, OutSize;

    protected:
	const CompressAPI& compress_api_;
};


void strtoupper(char *src) {
    int i;

    for (i = 0; i < strlen(src); i++)
	*src = _toupper(*src);
}


int main(int argc, char* argv[]) {
    int ret;
    char *p1, *p2;

    std::vector<FileHandler> file_handlers {
	FileHandler{(char *) ".RLE", CompressAPI_RLE{}},
	FileHandler{(char *) ".LZM", CompressAPI_LZMIT{}}
    };

    if (argc != 3) {
	printf("Invalid number of arguments\n");
	return 1;
    }

    if (!strlen(argv[1]) && !strlen(argv[2])) {
	printf("Invalid file name\n");
	return 2;
    }

    for (auto& handler: file_handlers) {
	strtoupper(argv[1]);
	strtoupper(argv[2]);
	p1 = strstr(argv[1], handler.GetExt());
	p2 = strstr(argv[2], handler.GetExt());
	if ((p1 != NULL) && (p2 == NULL)) {
		/* Found a match for the source file. */
		ret = handler.Read(argv[1]);
		if (!ret) {
			printf("Error reading file\n");
			return 3;
		}
		handler.Decompress();
		ret = handler.Write(argv[2]);
		if (!ret) {
			printf("Error writing file\n");
			return 4;
		}
		handler.Close();
	} else if ((p1 == NULL) && (p2 != NULL)) {
		/* Found a match for the source file. */
		ret = handler.Read(argv[1]);
		if (!ret) {
			printf("Error reading file\n");
			return 5;
		}
		handler.Compress();
		ret = handler.Write(argv[2]);
		if (!ret) {
			printf("Error writing file\n");
			return 6;
		}
		handler.Close();
	} else if ((p1 != NULL) && (p2 != NULL)) {
		printf("Double match\n");
		return 7;
	}  else {
		printf("Invalid file types\n");
		return 8;
	}
    }

    printf("Done\n");
    return 0;
}
