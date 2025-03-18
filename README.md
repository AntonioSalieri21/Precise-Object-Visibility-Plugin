# Precise-Object-Visibility-Plugin

Plugin for precise measurement of visibility of an object. It is implemented for Unreal Engine 5.

When trying to measure object visibility and/or size, there exist a lot of methods: ray tracing, occlusion queries, screen space boundaries — but they are not precise, often have a lot of edge cases, and sometimes, completely useless.  
This plugin, instead, directly measures how much an object occupies the screen by using the stencil buffer.

