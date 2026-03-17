# Widget Factory

Generate UMG Widget Blueprints from JSON templates.

Define your UI layout in a JSON file — widget tree, properties, slot settings, variable bindings — and let Widget Factory create the complete Widget Blueprint automatically.

## Compatibility

- Unreal Engine 5.3, 5.4, 5.5, 5.6, 5.7+
- Windows / Mac / Linux (Editor only)
- Optional UnLua integration (auto-detected at compile time)

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


## License

This plugin is commercially licensed. See [LICENSE](LICENSE) for details.  
Available on [Fab](https://fab.com) (Unreal Engine Marketplace).

---

# Widget Factory（中文）

从 JSON 模板自动生成 UMG Widget Blueprint。

在 JSON 文件中定义 UI 布局（控件树、属性、Slot 设置、变量绑定），Widget Factory 自动创建完整的 Widget Blueprint。

## 兼容性

- Unreal Engine 5.3、5.4、5.5、5.6、5.7+
- Windows / Mac / Linux（仅编辑器）
- 可选 UnLua 集成（编译时自动检测）

## 快速上手

1. 在 `.uproject` 中启用插件
2. 在 `Config/WidgetTemplates/` 创建 JSON 模板
3. 编辑器中：**工具 → Widget Factory** → 选择模板 → 生成

## 使用方式

### 编辑器窗口
**工具 → Widget Factory** 打开面板，可以：
- 查看 `Config/WidgetTemplates/` 下所有模板
- 选择要生成的模板
- 设置输出路径（默认 `/Game/UI`）
- 直接打开模板文件夹

### 控制台命令
```
WidgetFactory.Generate ShopPanel              # 生成到 /Game/UI
WidgetFactory.Generate ShopPanel /Game/Widgets # 自定义输出路径
WidgetFactory.GenerateAll                     # 生成所有模板
```

## 许可证

本插件为商业授权，详见 [LICENSE](LICENSE)。  
可在 [Fab](https://fab.com)（Unreal Engine 商城）购买。
