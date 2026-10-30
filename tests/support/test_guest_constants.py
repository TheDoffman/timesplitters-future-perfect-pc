"""Fixture data follows original memory operands, including SIMD widths."""
from guest_constants import absolute_data_words


def test_absolute_scalar_and_vector_constants_exclude_registers_and_code():
    # movss xmm0,[0x475cd4]; movups xmm1,[0x476000]; movss xmm0,[eax+0x475cd4];
    # mov eax,0x475cd4 (immediate); mov eax,[0x900000] (outside data range);
    # mov eax,fs:[0x476100] (segment-relative).
    image = bytes.fromhex('f30f1005d45c4700 0f100d00604700 f30f1080d45c4700 '
                          'b8d45c4700 a100009000 64a100614700')
    assert absolute_data_words(image, [(0x1000, 0x1000+len(image))], lambda va: va-0x1000) == [
        0x475cd4, 0x476000, 0x476004, 0x476008, 0x47600c]


def test_only_selected_routines_supply_constants():
    image = bytes.fromhex('a100604700 a104604700')
    assert absolute_data_words(image, [(0x1005, 0x100a)], lambda va: va-0x1000) == [0x476004]
