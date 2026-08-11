#include <gst/gst.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <cstdlib>

static int frame_count = 0;

static GstFlowReturn on_new_sample(GstElement *sink, gpointer user_data)
{
    GstSample *sample = nullptr;

    g_signal_emit_by_name(sink, "pull-sample", &sample);

    if (sample)
    {
        frame_count++;

        gst_sample_unref(sample);

        if (frame_count >= 120)
        {
            GstElement *pipeline = GST_ELEMENT(user_data);

            gst_element_send_event(
                pipeline,
                gst_event_new_eos()
            );
        }

        return GST_FLOW_OK;
    }

    return GST_FLOW_ERROR;
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    std::string device = "/dev/video4";

    if (argc > 1)
        device = argv[1];

    const int width = 1920;
    const int height = 1080;
    const int fps = 30;

    std::string pipeline_string =
        "v4l2src device=" + device +
        " ! video/x-raw,width=" +
        std::to_string(width) +
        ",height=" +
        std::to_string(height) +
        ",framerate=" +
        std::to_string(fps) +
        "/1 "
        "! videoconvert "
        "! tee name=t "
        "t. ! queue "
        "! x264enc tune=zerolatency "
        "! h264parse "
        "! mp4mux "
        "! filesink location=capture.mp4 "
        "t. ! queue "
        "! appsink name=sink emit-signals=true sync=false max-buffers=1 drop=false";

    GError *error = nullptr;

    GstElement *pipeline =
        gst_parse_launch(pipeline_string.c_str(), &error);

    if (!pipeline)
    {
        std::cerr << "Failed to create GStreamer pipeline\n";

        if (error)
        {
            std::cerr << error->message << "\n";
            g_error_free(error);
        }

        return 1;
    }

    GstElement *sink =
        gst_bin_get_by_name(GST_BIN(pipeline), "sink");

    g_signal_connect(
        sink,
        "new-sample",
        G_CALLBACK(on_new_sample),
        pipeline
    );

    auto start = std::chrono::steady_clock::now();

    std::cout << "Starting camera test\n";
    std::cout << "Device     : " << device << "\n";
    std::cout << "Resolution : "
              << width << "x" << height << "\n";
    std::cout << "FPS        : " << fps << "\n";
    std::cout << "Target     : 120 frames\n";

    gst_element_set_state(
        pipeline,
        GST_STATE_PLAYING
    );

    GstBus *bus =
        gst_element_get_bus(pipeline);

    bool success = false;

    while (true)
    {
        GstMessage *message =
            gst_bus_timed_pop_filtered(
                bus,
                GST_CLOCK_TIME_NONE,
                static_cast<GstMessageType>(
                    GST_MESSAGE_ERROR |
                    GST_MESSAGE_EOS
                )
            );

        if (!message)
            continue;

        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS)
        {
            success = true;
            gst_message_unref(message);
            break;
        }

        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR)
        {
            GError *err = nullptr;
            gchar *debug = nullptr;

            gst_message_parse_error(
                message,
                &err,
                &debug
            );

            std::cerr << "GStreamer ERROR: "
                      << err->message << "\n";

            if (debug)
                std::cerr << debug << "\n";

            g_error_free(err);
            g_free(debug);

            gst_message_unref(message);

            break;
        }

        gst_message_unref(message);
    }

    auto end = std::chrono::steady_clock::now();

    double seconds =
        std::chrono::duration<double>(
            end - start
        ).count();

    gst_element_set_state(
        pipeline,
        GST_STATE_NULL
    );

    gst_object_unref(bus);
    gst_object_unref(sink);
    gst_object_unref(pipeline);

    double actual_fps =
        seconds > 0 ?
        frame_count / seconds :
        0;

    std::ofstream log("test.log");

    log << "Camera Test\n";
    log << "============\n";
    log << "Device: " << device << "\n";
    log << "Resolution: "
        << width << "x" << height << "\n";
    log << "Requested FPS: " << fps << "\n";
    log << "Frames captured: "
        << frame_count << "\n";
    log << "Capture time: "
        << seconds << " seconds\n";
    log << "Measured FPS: "
        << actual_fps << "\n";

    if (success && frame_count == 120)
    {
        log << "RESULT: PASS\n";

        std::cout << "RESULT: PASS\n";

        return 0;
    }

    log << "RESULT: FAIL\n";

    std::cout << "RESULT: FAIL\n";

    return 1;
}
