// PreviewViewController.swift
// HyperXTalkQL — Quick Look preview extension for HyperXTalk files.
//
// .hyperxtalk files: reads the PNG snapshot stored in the
//   "com.hyperxtalk.qlpreview" extended attribute (written by the engine
//   on each save) and displays it as an image.
//
// .hyperxtalkscript files: reads the file as plain text and displays it
//   in a scrollable, monospaced text view.

import Cocoa
import Quartz

class PreviewViewController: NSViewController, QLPreviewingController {

    // No storyboard — create the view programmatically.
    override func loadView() {
        view = NSView(frame: NSRect(x: 0, y: 0, width: 512, height: 512))
    }

    private static let xattrName = "com.hyperxtalk.qlpreview"

    // MARK: - QLPreviewingController

    func preparePreviewOfFile(at url: URL,
                              completionHandler handler: @escaping (Error?) -> Void) {
        switch url.pathExtension.lowercased() {
        case "hyperxtalkscript":
            showScript(from: url)
        case "hyperxtalk":
            // Stack files store the preview as the com.hyperxtalk.qlpreview xattr.
            if let image = loadPreviewImage(from: url) {
                showImage(image)
            } else {
                showPlaceholder()
            }
        case "hyperxtalk_project":
            // Project packages contain a preview.png file inside the bundle.
            if let image = loadPreviewImageFromBundle(at: url) {
                showImage(image)
            } else {
                showPlaceholder()
            }
        default:
            showPlaceholder()
        }
        handler(nil)
    }

    // MARK: - Script text display

    private func showScript(from url: URL) {
        let text: String
        do {
            text = try String(contentsOf: url, encoding: .utf8)
        } catch {
            showPlaceholder()
            return
        }

        // Scrollable text view sized to fill the preview panel.
        let scrollView = NSScrollView(frame: view.bounds)
        scrollView.autoresizingMask = [.width, .height]
        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalScroller = false
        scrollView.borderType = .noBorder

        let textView = NSTextView(frame: scrollView.contentView.bounds)
        textView.autoresizingMask = [.width]
        textView.isEditable = false
        textView.isSelectable = true
        textView.isRichText = false

        // Monospaced font, comfortable size for a preview.
        textView.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        textView.textColor = NSColor.labelColor
        textView.backgroundColor = NSColor.textBackgroundColor

        // Reasonable left/right padding.
        textView.textContainerInset = NSSize(width: 8, height: 8)
        textView.textContainer?.widthTracksTextView = true
        textView.textContainer?.containerSize = NSSize(
            width: scrollView.contentView.bounds.width,
            height: CGFloat.greatestFiniteMagnitude
        )

        textView.string = text

        scrollView.documentView = textView
        view.addSubview(scrollView)
    }

    // MARK: - Stack image loading

    private func loadPreviewImageFromBundle(at url: URL) -> NSImage? {
        let previewURL = url.appendingPathComponent("preview.png")
        return NSImage(contentsOf: previewURL)
    }

    private func loadPreviewImage(from url: URL) -> NSImage? {
        let path = url.path
        let xattr = Self.xattrName

        let size = getxattr(path, xattr, nil, 0, 0, 0)
        guard size > 0 else { return nil }

        var buffer = [UInt8](repeating: 0, count: size)
        let read = getxattr(path, xattr, &buffer, size, 0, 0)
        guard read == size else { return nil }

        let data = Data(bytes: buffer, count: size)
        return NSImage(data: data)
    }

    // MARK: - View helpers

    private func showImage(_ image: NSImage) {
        let imageView = NSImageView(frame: view.bounds)
        imageView.image = image
        imageView.imageScaling = .scaleProportionallyUpOrDown
        imageView.autoresizingMask = [.width, .height]
        view.addSubview(imageView)
    }

    private func showPlaceholder() {
        // Stack exists but has no embedded preview, or script could not be read.
        // Fall back to the file icon.
        let iconView = NSImageView(frame: view.bounds)
        let fileType = "hyperxtalk"
        iconView.image = NSWorkspace.shared.icon(forFileType: fileType)
        iconView.imageScaling = .scaleProportionallyDown
        iconView.autoresizingMask = [.width, .height]
        view.addSubview(iconView)
    }
}
