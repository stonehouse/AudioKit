//
//  AKDynamicRangeCompressorTests.swift
//  AudioKitTestSuite
//
//  Created by Aurelius Prochazka on 7/14/17.
//  Copyright © 2017 AudioKit. All rights reserved.
//

import AudioKit
import XCTest

class AKDynamicRangeCompressorTests: AKTestCase {

    override func setUp() {
        super.setUp()
        // Need to have a longer test duration to allow for envelope to progress
        duration = 1.0
    }

    func testDefault() {
        input.amplitude = 2.0
        output = AKDynamicRangeCompressor(input)
        AKTestMD5("b1c90acd05dee35b6451311a2728b943")
    }

    func testParameters() {
        input.amplitude = 2.0
        output = AKDynamicRangeCompressor(input,
                                          ratio: 0.5,
                                          threshold: -1,
                                          attackTime: 0.2,
                                          releaseTime: 0.2)
        AKTestMD5("63d214d9dd66c9221490b62e2c45c6ad")
    }

    func testRatio() {
        input.amplitude = 2.0
        output = AKDynamicRangeCompressor(input, ratio: 0.5)
        AKTestMD5("52464bd10d726416467f0607c54074ab")
    }

    func testThreshold() {
        input.amplitude = 2.0
        output = AKDynamicRangeCompressor(input, ratio: 0.5, threshold: -1)
        AKTestMD5("c100407f30c24faea8a4bd4f1dbd091b")
    }

    func testAttackTime() {
        input.amplitude = 2.0
        output = AKDynamicRangeCompressor(input, ratio: 0.5, attackTime: 0.2)
        AKTestMD5("bce27dec5de103b9ceb078716a521268")
    }

    func testReleaseTime() {
        input.amplitude = 2.0
        output = AKDynamicRangeCompressor(input, ratio: 0.5, releaseTime: 0.2)
        AKTestMD5("de97410e4bcb3e7b17e18ea6f1f68a31")
    }

}
