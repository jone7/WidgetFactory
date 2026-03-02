# Widget Factory

Generate UMG Widget Blueprints from JSON templates.

Define your UI layout in a JSON file — widget tree, properties, slot settings, variable bindings — and let Widget Factory create the complete Widget Blueprint automatically.

## Quick Start

1. Enable the plugin in your `.uproject`
2. Create a JSON template in `Config/WidgetTemplates/`
3. In the editor: **Tools → Widget Factory** → select templates → Generate

## Usage

### Editor Window
**Tools → Widget Factory** opens a panel where you can:
- See all templates in `Config/WidgetTemplates/`
- Select which ones to generate
- Set the output content path (default `/Game/UI`)
- Open the template folder directly

### Console Commands
```
WidgetFactory.Generate ShopPanel              # generates to /Game/UI
WidgetFactory.Generate ShopPanel /Game/Widgets # custom output path
WidgetFactory.GenerateAll                     # all templates
```

### Python (Editor Utility)
```python
unreal.WidgetFactoryGenerator.generate_from_json("ShopPanel")
unreal.WidgetFactoryGenerator.generate_from_json("ShopPanel", "/Game/Widgets")
unreal.WidgetFactoryGenerator.generate_all_widgets()
```

## JSON Template Format

```jsonc
{
  "WidgetName": "WBP_MyPanel",       // Blueprint asset name
  "Description": "My cool panel",    // shown in editor window
  "RootWidget": {
    "Type": "CanvasPanel",           // widget class name
    "Name": "Root",
    "Children": [
      {
        "Type": "TextBlock",
        "Name": "TitleText",
        "IsVariable": true,          // expose as blueprint variable
        "Slot": {                    // layout slot properties
          "Anchors": "Center",
          "Padding": { "Left": 10, "Top": 5, "Right": 10, "Bottom": 5 }
        },
        "Properties": {             // widget-specific properties
          "Text": "Hello",
          "FontSize": 24,
          "Color": { "R": 1, "G": 1, "B": 1, "A": 1 },
          "AutoWrap": true
        }
      }
    ]
  }
}
```

## Supported Widget Types

**Containers:** CanvasPanel, VerticalBox, HorizontalBox, ScrollBox, Overlay, SizeBox, ScaleBox, Border, GridPanel, UniformGridPanel, WrapBox, WidgetSwitcher

**Leaf widgets:** Button, TextBlock, RichTextBlock, Image, Spacer, ProgressBar, Slider, CheckBox, EditableText, EditableTextBox, ComboBoxString, Throbber, CircularThrobber

## Slot Properties

Slot properties depend on the parent container:

### CanvasPanel children
| Property | Type | Example |
|----------|------|---------|
| Anchors | string | "Fill", "Center", "TopLeft", "BottomCenter", etc. |
| AnchorsCustom | object | `{ "MinX": 0, "MinY": 0, "MaxX": 1, "MaxY": 1 }` |
| Position | object | `{ "X": 100, "Y": 50 }` |
| Size | object | `{ "Width": 400, "Height": 300 }` |
| Alignment | object | `{ "X": 0.5, "Y": 0.5 }` |
| Offsets | object | `{ "Left": 0, "Top": 0, "Right": 0, "Bottom": 0 }` |
| AutoSize | bool | true |
| ZOrder | int | 1 |

### VerticalBox / HorizontalBox children
| Property | Type | Example |
|----------|------|---------|
| SizeRule | string | "Auto" or "Fill" |
| FillWidth / FillHeight | number | 0.5 |
| HAlign | string | "Left", "Center", "Right", "Fill" |
| VAlign | string | "Top", "Center", "Bottom", "Fill" |
| Padding | object | `{ "Left": 10, "Top": 5, "Right": 10, "Bottom": 5 }` |

## Widget Properties

### TextBlock
`Text`, `FontSize`, `FontFamily` (asset path), `Color`, `AutoWrap`, `Justification` ("Left"/"Center"/"Right")

### Image
`Color`, `Brush` (texture asset path)

### Spacer
`Size` (number, applied to both axes)

### ProgressBar
`Percent`, `FillColor`

### SizeBox
`WidthOverride`, `HeightOverride`, `MinDesiredWidth`, `MinDesiredHeight`, `MaxDesiredWidth`, `MaxDesiredHeight`

### All widgets
`Visibility` ("Visible"/"Collapsed"/"Hidden"/"HitTestInvisible"/"SelfHitTestInvisible"), `RenderOpacity`
