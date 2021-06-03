#include <iostream>
#include <Processing.NDI.Lib.h>
#include <ncurses.h>

static std::atomic<bool> exit_loop(false);

static void sigint_handler(int) {
    exit_loop = true;
}

float max_abs(float *start, const float *end) {
    float *iter = start;
    float max = *start;
    float abs_iter;
    while (++iter != end) {
        abs_iter = abs(*iter);
        if (abs_iter > max) {
            max = abs_iter;
        }
    }

    return max;
}

int main() {
    if (!NDIlib_initialize()) {
        std::cout << "Cannot run NDI.";
        return 0;
    }

    // Catch interrupt so that we can shut down gracefully
    signal(SIGINT, sigint_handler);

    // Create a finder
    const NDIlib_find_create_t NDI_find_create_desc;
    NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v2(&NDI_find_create_desc);
    if (!pNDI_find) return 0;

    // We wait until there is at least one source on the network
    uint32_t no_sources = 0;
    const NDIlib_source_t *p_sources = nullptr;
    while (!exit_loop && !no_sources) {
        NDIlib_find_wait_for_sources(pNDI_find, 1000);
        p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
    }

    // We need at least one source
    if (!p_sources) {
        return 0;
    }

    std::cout << "Select source:\n";
    for (int i = 0; i < no_sources; i++) {
        std::cout << i << " - " << p_sources[i].p_ndi_name << " (" << p_sources[i].p_ip_address << ")\n";
    }
    std::cout << "> ";
    int source;
    std::cin >> source;

    // We now have at least one source, so we create a receiver to look at it.
    NDIlib_recv_create_v3_t NDI_recv_create_desc;
    NDI_recv_create_desc.source_to_connect_to = p_sources[source];
    NDI_recv_create_desc.p_ndi_recv_name = "Volume Meter";
    NDI_recv_create_desc.bandwidth = NDIlib_recv_bandwidth_audio_only;

    // Create the receiver
    NDIlib_recv_instance_t pNDI_recv = NDIlib_recv_create_v3(&NDI_recv_create_desc);
    if (!pNDI_recv) {
        NDIlib_find_destroy(pNDI_find);
        return 0;
    }

    initscr();
    start_color();

    // Force bright color
    if (can_change_color()) {
        init_color(COLOR_GREEN, 0, 1000, 0);
        init_color(COLOR_YELLOW, 1000, 1000, 0);
        init_color(COLOR_RED, 1000, 0, 0);
    }

    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    attron(A_BOLD);
    printw(p_sources[source].p_ndi_name);

    // Destroy the NDI finder. We needed to have access to the pointers to p_sources[0]
    NDIlib_find_destroy(pNDI_find);


    float dB;
    // The RMS computation is currently commented-out, as it looks awful right now
    //float rms;
    float *display_dB = nullptr;
    float *peak_dB = nullptr;
    int64_t *peak_time = nullptr;

    // Run for one minute
    while (!exit_loop) {    // The descriptors
        NDIlib_audio_frame_v2_t audio_frame;

        switch (NDIlib_recv_capture_v2(pNDI_recv, nullptr, &audio_frame, nullptr, 1000)) {
            // Audio data

            case NDIlib_frame_type_audio: {
                // Allocate per-channel level storage
                if (!display_dB || !peak_dB || !peak_time) {
                    display_dB = new float[audio_frame.no_channels];
                    peak_dB = new float[audio_frame.no_channels];
                    peak_time = new int64_t[audio_frame.no_channels];
                    for (int c = 0; c < audio_frame.no_channels; c++) {
                        display_dB[c] = -INFINITY;
                        peak_dB[c] = -INFINITY;
                    }
                }

                for (int c = 0; c < audio_frame.no_channels; c++) {
                    // Decay programme display 1 dB per frame
                    display_dB[c] -= 1;
                    // Hold peak for 10 s
                    if (audio_frame.timecode - peak_time[c] > 100000000) peak_dB[c] = -INFINITY;

                    // Compute programme peak in dB
                    dB = log10(max_abs(audio_frame.p_data + c * audio_frame.no_samples,
                                       audio_frame.p_data + (c + 1) * audio_frame.no_samples)) * 20;

                    /*
                    rms = 0;
                    for (float *it = audio_frame.p_data + c * audio_frame.no_samples;
                         it < audio_frame.p_data + (c + 1) * audio_frame.no_samples; it += sizeof(float)) {
                        rms += pow(*it, 2);
                    }
                    rms = log10(sqrt(rms / (float) audio_frame.no_samples)) * 20;
                    */

                    // Update programme display and peak
                    if (dB > display_dB[c]) display_dB[c] = dB;
                    if (dB > peak_dB[c]) {
                        peak_dB[c] = dB;
                        peak_time[c] = audio_frame.timecode;
                    }

                    // Draw volume meter
                    move(c + 1, 0);
                    attron(COLOR_PAIR(1));
                    for (int x = -COLS; x < 0; x++) {
                        if (x == -20) attron(COLOR_PAIR(2));
                        else if (x == -9) attron(COLOR_PAIR(3));

                        /*
                        if (floor(rms) == (float) x) printw("-");
                        else */
                        if (display_dB[c] > (float) x) printw("=");
                        else if (floor(peak_dB[c]) == (float) x) printw("|");
                        else printw(".");
                    }
                    attroff(COLOR_PAIR(3));
                }
                mvprintw(audio_frame.no_channels+2, 0, "Time: %10u\n", audio_frame.timecode);
                refresh();

                // Free the original buffer
                NDIlib_recv_free_audio_v2(pNDI_recv, &audio_frame);
                break;
            }

            default:
                break;
        }
    }

    delete[] display_dB;
    delete[] peak_dB;
    delete[] peak_time;

    endwin();

    // Destroy the receiver
    NDIlib_recv_destroy(pNDI_recv);

    // Not required, but nice
    NDIlib_destroy();

    // Finished
    return 0;
}
