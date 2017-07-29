//: ## Modal Resonance Filter
//:
import AudioKitPlaygrounds
import AudioKit

let file = try AKAudioFile(readFileName: playgroundAudioFiles[0], baseDir: .resources)

let player = try AKAudioPlayer(file: file)
player.looping = true

var filter = AKModalResonanceFilter(player)
filter.frequency = 300 // Hz
filter.qualityFactor = 20

let balancedOutput = AKBalancer(filter, comparator: player)
AudioKit.output = balancedOutput
AudioKit.start()

player.play()

//: User Interface Set up

class PlaygroundView: AKPlaygroundView {

    override func setup() {
        addTitle("Modal Resonance Filter")

        addSubview(AKResourcesAudioFileLoaderView(player: player, filenames: playgroundAudioFiles))

        addSubview(AKPropertySlider(property: "Frequency",
                                    value: filter.frequency,
                                    range: 0 ... 5_000,
                                    taper: 3,
                                    format: "%0.1f Hz"
        ) { sliderValue in
            filter.frequency = sliderValue
        })

        addSubview(AKPropertySlider(property: "Quality Factor",
                                    value: filter.qualityFactor,
                                    range: 0.1 ... 20,
                                    format: "%0.1f"
        ) { sliderValue in
            filter.qualityFactor = sliderValue
        })

    }
}

import PlaygroundSupport
PlaygroundPage.current.needsIndefiniteExecution = true
PlaygroundPage.current.liveView = PlaygroundView()
