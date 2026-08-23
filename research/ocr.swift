import Foundation
import Vision
import AppKit

// Usage: swift ocr.swift <image-path> [output-path]
// OCRs the image with Vision and prints recognized text with bounding boxes.

let args = CommandLine.arguments
guard args.count >= 2 else {
    print("usage: swift ocr.swift <image> [out]")
    exit(1)
}

let path = args[1]
guard let img = NSImage(contentsOfFile: path),
      let cg = img.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
    print("cannot load image")
    exit(1)
}

let request = VNRecognizeTextRequest { req, err in
    guard let observations = req.results as? [VNRecognizedTextObservation] else { return }
    // Sort top-to-bottom, then left-to-right by bounding box
    let sorted = observations.sorted { a, b in
        let ay = a.boundingBox.midY, by = b.boundingBox.midY
        if abs(ay - by) > 0.015 { return ay > by }
        return a.boundingBox.minX < b.boundingBox.minX
    }
    var out: [String] = []
    for obs in sorted {
        guard let top = obs.topCandidates(1).first else { continue }
        let bb = obs.boundingBox
        let line = String(format: "[y=%.3f x=%.3f] %@", bb.midY, bb.minX, top.string)
        out.append(line)
    }
    let joined = out.joined(separator: "\n")
    print(joined)
    if args.count >= 3 {
        try? joined.write(toFile: args[2], atomically: true, encoding: .utf8)
    }
}

request.recognitionLevel = .accurate
request.usesLanguageCorrection = false
request.recognitionLanguages = ["en-US"]

let handler = VNImageRequestHandler(cgImage: cg, options: [:])
try? handler.perform([request])
