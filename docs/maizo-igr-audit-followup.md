# In-Game PRO: audit follow-up (experimental)
 
Based on audit of a93193e; retain Setup Dash and Correction B.
 
- Existing alarm promotes the sleeping monitor to priority 0. This is not a new periodic interrupt or an ISR pad parser. It cannot help if the title disables alarms/interrupts or monopolizes an equal-priority context.
- Pause eligible EE threads before draining the hardware, leaving completion IRQs enabled. Acquire the existing graphics IRQ mask only after a valid idle sample; recheck with the mask held. Retry with game threads restored.
- Do not guess CRTC state from privileged MMIO reads. Require recorder-backed display state.
- No claimed universal quiescence: VU1 explicit busy checks need verified safe COP2 context; IRQs outside the graphics hold may still produce packets. PATH2 and existing partial-packet failure handling remain experimental. Do not claim that timeout recovery is safe.
- No shadow front buffer: observing alternating FBP addresses does not establish that a back buffer is disposable. A safe pixel backup and restoration protocol is still needed.
- No universal commercial GIF interception or changes to legacy IGR. Validate boot and repeated open/close on real PS2, including 3D, FMV, and PATH3-masked titles.
