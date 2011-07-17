#include <bitcoin/script.hpp>
#include <bitcoin/util/ripemd.hpp>
#include <iostream>

void ripemd_test()
{
    libbitcoin::data_chunk test_data{'h', 'e', 'l', 'l', 'o'};
    libbitcoin::short_hash hash = libbitcoin::generate_ripemd_hash(test_data);
    for (auto it = hash.begin(); it != hash.end(); ++it)
        std::cout << std::hex << (int)*it << " ";
    std::cout << "\n";
}

int main()
{
    // from tx 31ef018c55dad667e2c2e276fbb641f4b6ace07ca57fdcb86cb4b9a8ff7f20eb
    libbitcoin::data_chunk raw_input{0x48, 0x30, 0x45, 0x02, 0x21, 0x00, 0xfa, 0xe3, 0x07, 0x0f, 0xef, 0xd3, 0x12, 0x7a, 0x10, 0x18, 0xfb, 0xbb, 0x1a, 0x39, 0x25, 0xc2, 0x46, 0xc0, 0x89, 0x77, 0x14, 0x17, 0x3f, 0xf4, 0x8b, 0xbe, 0xfa, 0x9e, 0xa2, 0xfe, 0x72, 0xf9, 0x02, 0x20, 0x20, 0xc8, 0xcc, 0xa0, 0x48, 0x06, 0x83, 0xd4, 0x16, 0x0c, 0x16, 0x2d, 0x2c, 0xfe, 0xe8, 0xf3, 0x02, 0x52, 0x65, 0xa9, 0x3b, 0x8f, 0xd4, 0xad, 0x2f, 0xd9, 0xd8, 0x32, 0xc6, 0x57, 0x28, 0x50, 0x01, 0x41, 0x04, 0x36, 0x73, 0xc1, 0x84, 0x06, 0xd7, 0x92, 0xe7, 0x92, 0x28, 0xde, 0xff, 0x1e, 0xd9, 0xe3, 0x7e, 0x62, 0x4a, 0x93, 0x0a, 0x18, 0x7c, 0x6e, 0x43, 0x98, 0x0a, 0xe0, 0xeb, 0xc4, 0x7d, 0x5a, 0x84, 0xcf, 0x05, 0x5a, 0x35, 0x7a, 0x73, 0x60, 0xe2, 0xb4, 0x48, 0x80, 0x48, 0xdb, 0x7d, 0x28, 0x08, 0x0d, 0x33, 0x38, 0x57, 0xa8, 0x1e, 0x1c, 0x40, 0x66, 0x1c, 0x8f, 0x61, 0xd3, 0x79, 0xc9, 0x5c};
    assert(raw_input[0] + raw_input[raw_input[0] + 1] == raw_input.size() - 2);
    libbitcoin::data_chunk raw_script{0x76, 0xa9, 0x14, 0xe7, 0x58, 0x5a, 0x2d, 0xc9, 0xd9, 0xcb, 0x01, 0xd5, 0x47, 0x1d, 0xec, 0xf2, 0x39, 0xeb, 0xbd, 0xdd, 0xb6, 0x9b, 0x32, 0x88, 0xac};
    libbitcoin::script script = libbitcoin::parse_script(raw_input);
    libbitcoin::script script_out = libbitcoin::parse_script(raw_script);
    script.join(script_out);
    std::cout << script.string_repr() << "\n";
    libbitcoin::transaction tx;
    std::cout << "Returned: " << (script.run(tx) ? "true" : "false") << "\n";
    return 0;
}
