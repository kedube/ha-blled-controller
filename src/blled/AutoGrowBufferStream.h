//Code from Xtouchautogrowstream
#ifndef AutoGrowBufferStream_h
#define AutoGrowBufferStream_h

#include <Arduino.h>
#include <Stream.h>

#define BUFFER_INCREMENTS 1024
#define MAX_BUFFER_SIZE 65536

// Collects one MQTT message at a time. The buffer keeps its size between messages: printer
// reports are similar in size, and shrinking and regrowing it for every report fragments the
// heap until a report no longer fits.
class AutoGrowBufferStream : public Stream
{
private:
    size_t _len;
    size_t buffer_size;
    char* _buffer;
    bool _overflow;  // this message didn't fit; the rest of it is counted, not stored
    size_t _dropped;

public:
    AutoGrowBufferStream() {
        this->_len = 0;
        this->_buffer = (char*)malloc(BUFFER_INCREMENTS);
        this->buffer_size = BUFFER_INCREMENTS;
        this->_overflow = false;
        this->_dropped = 0;
    }

    ~AutoGrowBufferStream() {
        free(this->_buffer);
    }

    virtual size_t write(uint8_t byte) {
        if (this->_overflow) {
            this->_dropped++;
            return 0;
        }
        if (this->_len + 1 > this->buffer_size) {
            auto tmp = this->buffer_size + BUFFER_INCREMENTS <= MAX_BUFFER_SIZE
                           ? (char*)realloc(this->_buffer, this->buffer_size + BUFFER_INCREMENTS)
                           : NULL;
            if (tmp == NULL) {
                // The reader checks overflowed() and logs once per message, not once per byte.
                this->_overflow = true;
                this->_dropped = 1;
                return 0;
            }
            this->_buffer = tmp;
            this->buffer_size += BUFFER_INCREMENTS;
        }
        this->_buffer[this->_len] = byte;
        this->_len++;
        return 1;
    }

    virtual int read() {
        return 0;
    }

    virtual int available() {
        return 1;
    }

    // Ready for the next message; keeps the allocation (see above).
    virtual void flush() {
        this->_len = 0;
        this->_overflow = false;
        this->_dropped = 0;
    }

    int peek() {
        return 0;
    }

    bool overflowed() const { return _overflow; }
    // The whole message, including the part that didn't fit.
    size_t message_length() const { return _len + _dropped; }

    const size_t current_length() const { return _len; }
    const char* get_buffer() const { return _buffer; }
    const char* get_string() const { 
        _buffer[_len] = '\0';
        return _buffer;
    }

    using Print::write;
};

#endif
