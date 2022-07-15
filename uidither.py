#!/usr/bin/python3

import PySimpleGUI as sg
import sys, subprocess, os


# todo : Run dither once to extract this list at runtime
algoargs = {
    "nop": "None",
    "fs": "Floyd-Steinberg",
    "err1": "Diffuse large errors only"
}


options = [
]

inputside = [
    [
        sg.Text("Input image"),
        sg.Input(size=(25,1), enable_events=True, key="-INPUT FILENAME-"),
        sg.FileBrowse(),
    ],

    [
        sg.Text("Output image"),
        sg.Input(size=(25,1), enable_events=True, key="-OUTPUT FILENAME-", default_text="ditherout.png"),
        sg.FileBrowse(),
    ],


    [ sg.HSeparator() ],

    [ sg.Text("Pre-processing options:") ],
    [ sg.Text("Gamma: "), sg.Push(), sg.Slider(range=(0,3.0), default_value=1.0, key="-GAMMA-", orientation="horizontal", resolution=0.05, enable_events=True) ],
    [ sg.Text("Pre-gain bias: "), sg.Push(), sg.Slider(range=(-256,256), default_value=0, key="-PREBIAS-", orientation="horizontal", enable_events=True) ],
    [ sg.Text("Gain (%): "), sg.Push(), sg.Slider(range=(0,1500), default_value=100, key="-GAIN-", orientation="horizontal", enable_events=True) ],
    [ sg.Text("Post-gain bias: "), sg.Push(), sg.Slider(range=(-256,256), default_value=0, key="-POSTBIAS-", orientation="horizontal", enable_events=True) ],

    [ sg.HSeparator() ],
    [ sg.Text("Palette generation options:") ],
    [ sg.Text("Quantize depth: "), sg.Spin(values=[1,2,3,4,5,6,7,8], initial_value=4, size=(4,1), key="-BITDEPTH-", enable_events=True), sg.Text("(bits per color)") ],
    [ sg.Text("Maximum colors: "), sg.Spin(values=[i for i in range(1,256)], initial_value=256, size=(4,1), key="-MAXCOLORS-", enable_events=True) ],
    [ sg.Text("Dithering algorithm: "), sg.Combo(values=list(algoargs.values()), default_value="Floyd-Steinberg", key="-ALGO-" ) ],
    #[ sg.Text("Dithering algorithm: "), sg.Combo(values=["None","Floyd-Steinberg","Diffuse large errors only" ], default_value="Floyd-Steinberg", key="-ALGO-" ) ],

    [ sg.HSeparator() ],

    [ sg.Button("Apply", key="-APPLY-") ],
]

imagesSide = [
    [ sg.Text("Input") ],
    [ sg.Image(key="-IMAGEIN-")],
    [ sg.Text("Output:") ],
    [ sg.Image(key="-IMAGEOUT-")],
]

layout = [
    sg.vtop(
    [
        sg.Column(inputside),
        sg.VSeparator(),
        sg.Column(imagesSide),
    ]),
    [   sg.HSeparator() ],

    [ sg.Text("Last command:") ],
    [   sg.Multiline("", key="-COMMANDLINE-", expand_x = True, expand_y = True, size = (40,5)) ]
]

window = sg.Window("Ditherfront", layout, finalize=True, resizable=True)



if len(sys.argv) > 1:
    window["-INPUT FILENAME-"].update(sys.argv[1])
    window["-IMAGEIN-"].update(sys.argv[1])

if len(sys.argv) > 2:
    window["-OUTPUT FILENAME-"].update(sys.argv[2])
    window["-IMAGEOUT-"].update(sys.argv[2])

ditherTool=os.path.realpath("dither")

while True:
    event, values = window.read()


    if event == sg.WIN_CLOSED:
        break

    if event == "-INPUT FILENAME-":
        filename = values['-INPUT FILENAME-'];
        window["-IMAGEIN-"].update(filename=filename)

    #print("Event: ", event)

    autoUpdate = event in [ "-GAIN-","-PREBIAS-","-POSTBIAS-","-BITDEPTH-","-MAXCOLORS-","-GAMMA-","-ALGO-" ]

    if event == "-APPLY-" or autoUpdate:
        gain = str(values["-GAIN-"]/100.0)
        prebias = str(values["-PREBIAS-"])
        postbias = str(values["-POSTBIAS-"])
        gamma = str(values["-GAMMA-"])
        algo = str(values["-ALGO-"])


        idx = list(algoargs.values()).index(algo)
        algo = list(algoargs.keys())[idx]

        command = [
            ditherTool,
#            "-v",

            # Palette generation steps
            "-in", values["-INPUT FILENAME-"],

            "-gamma", gamma,
            "-bias", prebias,
            "-gain", gain,
            "-bias", postbias,
            "-quantize", str(values["-BITDEPTH-"]),
            "-reducecolors", str(values["-MAXCOLORS-"]),
            "-makepal", # make this the reference palette

            # Load the original file again, apply same preprocessing...
            "-reload",
            "-gamma", gamma,
            "-bias", prebias,
            "-gain", gain,
            "-bias", postbias,

            # Now dither using palette
            "-algo", algo,
            "-dither",

            # And output the result
            "-out", values["-OUTPUT FILENAME-"],
        ]

        #print(command)
        window["-COMMANDLINE-"].update(" ".join(command))
        subprocess.run(command);

        outfilename = values["-OUTPUT FILENAME-"]
        window["-IMAGEOUT-"].update(filename=outfilename)

window.close()

