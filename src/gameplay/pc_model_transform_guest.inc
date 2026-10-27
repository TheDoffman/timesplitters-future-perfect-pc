/* US 45410066 affine point transform, sub_00151E80. Called after the entry
 * callback; returning zero changes no guest state. The generated fallback
 * handles overlapping buffers and exceptional values. NaN result payloads
 * remain compiler-dependent in that unchanged generated arithmetic. */
static float model_transform_mul(float left, float right)
{
    volatile float result = left * right;
    return result;
}
static float model_transform_add(float left, float right)
{
    volatile float result = left + right;
    return result;
}

int pc_model_transform_entry(void)
{
    uint32_t matrix = MEM32(g_esp + 4u), point = MEM32(g_esp + 8u);
    if (matrix > UINT32_MAX - 59u || point > UINT32_MAX - 11u ||
        ((uint64_t)point < (uint64_t)matrix + 60u &&
         (uint64_t)matrix < (uint64_t)point + 12u))
        return 0;
    /* Bound products and sums away from overflow. Integer checks also reject
     * NaNs/infinities without raising new floating-point exceptions. */
    for (unsigned axis = 0; axis < 3; axis++) {
        if ((MEM32(point + 4u * axis) & 0x7fffffffu) > 0x50000000u)
            return 0;
        for (unsigned row = 0; row < 4; row++)
            if ((MEM32(matrix + 16u * row + 4u * axis) & 0x7fffffffu) > 0x50000000u)
                return 0;
    }
    float x = MEMF(point), y = MEMF(point + 4u), z = MEMF(point + 8u);
    float x_term = 0.f, y_term = 0.f, result = 0.f, middle_x = 0.f;
    for (unsigned axis = 0; axis < 3; axis++) {
        uint32_t offset = 4u * axis;
        float z_term = model_transform_mul(MEMF(matrix + 32u + offset), z);
        y_term = model_transform_mul(MEMF(matrix + 16u + offset), y);
        result = model_transform_add(z_term, y_term);
        x_term = model_transform_mul(MEMF(matrix + offset), x);
        result = model_transform_add(result, x_term);
        result = model_transform_add(result, MEMF(matrix + 48u + offset));
        MEMF(point + offset) = result;
        if (axis == 1) middle_x = x_term;
    }
    /* The original leaves xmm4 at its middle-row x product, not its final row. */
    g_eax = matrix;
    g_ecx = point;
    g_xmm0 = XMM_SCALAR(x);
    g_xmm1 = XMM_SCALAR(x_term);
    g_xmm2 = XMM_SCALAR(y_term);
    g_xmm3 = XMM_SCALAR(result);
    g_xmm4 = XMM_SCALAR(middle_x);
    g_esp += 4u;
    return 1;
}
