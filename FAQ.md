# FAQs

## Why port another firmware to the MNT Reform keyboard?

I've been keeping an eye on the hardware being put out by MNT Research for a
few years now. When I saw the MNT Reform Next announcement I jumped at the
opportunity to join the crowdfunding campaign.

As a mechanical keyboard enthusiast, there are certain comforts I've come to
appreciate that are hard for me to live without at this point. Keyboards being
the primary input interface between the user and computer are deeply personal
and everyone has their own preferences for layout and behavior. This type of
customization should not be reserved for technical users that can modify and
recompile firmware.

## Why not QMK?

When I began this journey, my intent was to port QMK/Vial.

After backing the MNT Reform Next campaign I decided to pick up one of the new
standalone keyboards to get a headstart on working porting to QMK. Before the
keyboard had even shipped, I had a branch of QMK ready to go and test as soon
as it arrived. Aside from a couple misconfigured pins, all the hardware worked
immediately! Until it didn't... the USB device would stop responding after a
while.

After a week or so of researching and testing, I had made effectively no
progress. I found QMK to be difficult to debug and get meaningful information
out of -- especially as when the USB cut out, so did all of the logging. Having
difficulties debugging under QMK, I decided to test other firmwares as well to
help determine if the issue was specific to QMK or not. Through this, I ended
up testing out KMK, and ZMK -- both of which I had no experience with. In the
end, all three firmwares showed the same issue and I still had no answer for
what was wrong.

At this point, I had to make decision about which firmware was the most
worthwhile to spend a significant amount of time and effort into debugging and
the only firmware that I was actually able to make significant progress with
was ZMK. ZMK allowed me to easily redefine the hardware definition to use the
existing UART port on the keyboard as a serial console for logging. This has
been invaluable as I debug these USB issues. The more I researched into ZMK,
the more strongly I felt that it's the better basis to build on top of.

## Why use ZMK?

1) **Project Structure**

QMK is a monolithic repository. To truly add support to QMK for a keyboard, you
need to upstream your definition. This is a relatively straightforward process
for basic wired keyboards. For the MNT Reform keyboard, we're going to need a
non-trivial amount of custom logic to support the OLED menu system and system
controller communication. This kind of non-standard, project specific logic
will be difficult to upstream into QMK and add a lot of friction to the
development cycle. Frankly, I do not wish to maintain a fork of QMK to meet my
goals.

ZMK, being built on top of Zephyr, allow for external module definitions. This
means that all of the hardware definitions and custom behaviors can exist in a
dedicated repository and I don't have to fork ZMK. This also means there's no
friction with external maintainers.

2) **Supported features & priorities**

ZMK was designed from the ground up for wireless keyboards. While the MNT
Reform keyboard is not wireless, it is designed to be put into a laptop meaning
they share a common design constraint: they run on a battery. Power consumption
optimization is a neccesity, not an afterthought. It also means that if a
future revision or variant wanted to add wireless support it should be easy to
do.

3) **Keymap customization via a GUI**

VIA is currently the gold standard for graphical keyboard management. However,
to get supported by VIA the following must happen:

* Upstream your keyboard definition into QMK
* Upstream keyboard definition into VIA repositories

Both of these things will likely be extremely difficult for this project.
Graphical editor support is a primary goal of this project and this makes it
difficult for me to want to pursue QMK/VIA. I also fundamentally disagree with
VIA's approach from a technical and philosophical standpoint. Requiring a
centralized repository for definitions to be supported by the GUI editor is
simply unnecessary friction. Vial and ZMK Studio do not require explicit
support by the editor itself, proving this is unnecessary.

4) **Zephyr base**

ZMK is built on top of the open source RTOS Zephyr. Zephyr makes it really easy
to get access to lower level details of the firmware and make modifications if
needed. Zephyr has good support for various hardware, that's abstracted well,
meaning this firmware should be more portable to different MCUs in the future
if required. Additionally, almost all of the work to define the hardware to
work under ZMK, is applicable to Zephyr. This means we can use a lot of the
work here to build things on top of Zephyr directly in the future.
