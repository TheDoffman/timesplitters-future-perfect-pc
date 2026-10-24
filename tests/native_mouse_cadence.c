/* Exercise actual shared input with packet arrivals independent of rendering.
 * This models delivery cadence, not OS input or end-to-end display latency. */
#include "../src/pc_input.c"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static int cadence(unsigned rate, unsigned packet_rate, int jitter, float zoom, int inverted)
{
    PcInput input = {0};
    float yaw = 0, pitch = 0;
    unsigned packet = 1, changes = 0;
    pc_input_focus(&input, 1);
    pc_input_capture(&input, 1);
    /* One second, exactly 1,000 horizontal and 250 vertical counts. */
    for (unsigned frame = 1; frame <= rate; ++frame) {
        uint64_t due_us = (uint64_t)frame * 1000000 / rate;
        if (jitter && frame < rate)
            due_us += frame % 3 == 0 ? 2000 : frame % 3 == 1 ? -2000 : 0;
        while ((uint64_t)packet * 1000000 <= due_us * packet_rate) {
            pc_input_motion(&input, 1000.0 / packet_rate, 250.0 / packet_rate,
                            1000 + packet * 1000 / packet_rate);
            ++packet;
        }
        uint64_t now = 1000 + due_us / 1000;
        changes += pc_input_look(&input, now, .1f, inverted, zoom, &yaw, &pitch);
        CHECK(!pc_input_look(&input, now, .1f, inverted, zoom, &yaw, &pitch));
    }
    printf("mouse %u Hz / updates %u Hz jitter %d zoom %.1f inverted %d: yaw %.6f pitch %.6f changed %u\n",
           packet_rate, rate, jitter, zoom, inverted, yaw, pitch, changes);
    CHECK(packet == packet_rate + 1);
    return fabs(yaw + 100 * zoom) < .003 &&
           fabs(pitch - (inverted ? 25 : -25) * zoom) < .003;
}

static void interruptions(void)
{
    PcInput input = {0};
    float yaw = 0, pitch = 0;
    pc_input_focus(&input, 1);
    pc_input_capture(&input, 1);
    pc_input_motion(&input, 10, 0, 1000);
    CHECK(!pc_input_look(&input, 1150, .1f, 0, 1, &yaw, &pitch));
    CHECK(yaw == 0 && input.dx == 0);
    pc_input_motion(&input, 10, 0, 1151);
    CHECK(pc_input_look(&input, 1152, .1f, 0, 1, &yaw, &pitch));
    CHECK(fabs(yaw + 1) < .001);
    /* Continuous movement while no camera consumes it (e.g. cutscene) stays
       bounded. Do not accumulate the entire second for the gameplay return. */
    for (unsigned t = 1153; t <= 2152; ++t)
        pc_input_motion(&input, 1, 0, t);
    CHECK(input.dx > 0 && input.dx <= 51);
    float pending = (float)input.dx;
    CHECK(pc_input_look(&input, 2152, .1f, 0, 1, &yaw, &pitch));
    CHECK(fabs(yaw + 1 + pending * .1f) < .001);
    pc_input_motion(&input, 20, 0, 2153);
    pc_input_focus(&input, 0);
    pc_input_motion(&input, 500, 0, 2154);
    pc_input_focus(&input, 1);
    pc_input_capture(&input, 1);
    CHECK(!pc_input_look(&input, 2155, .1f, 0, 1, &yaw, &pitch));
    pc_input_motion(&input, 20, 0, 2156);
    pc_input_capture(&input, 0);
    pc_input_capture(&input, 1);
    CHECK(!pc_input_look(&input, 2157, .1f, 0, 1, &yaw, &pitch));
}

int main(void)
{
    const unsigned rates[] = {60, 120, 144, 180, 240};
    const unsigned packets[] = {125, 500, 1000, 8000};
    unsigned failures = 0;
    for (unsigned i = 0; i < sizeof rates / sizeof *rates; ++i)
        for (unsigned j = 0; j < sizeof packets / sizeof *packets; ++j)
            for (int jitter = 0; jitter <= 1; ++jitter)
                for (int zoom = 0; zoom <= 1; ++zoom)
                    failures += !cadence(rates[i], packets[j], jitter, zoom ? .5f : 1, zoom);
    interruptions();
    CHECK(!failures);
    return 0;
}
