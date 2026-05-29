import plantuml as pu

PLANTUML_SERVER = "http://www.plantuml.com/plantuml"


def plantuml_formatter(source, language, css_class, options, md, attrs, **kwargs):
    try:
        encoded = pu.deflate_and_encode(source)
        url = f"{PLANTUML_SERVER}/svg/{encoded}"
        return f'<div class="plantuml"><img alt="PlantUML diagram" src="{url}" /></div>'
    except Exception as e:
        return f'<pre><code>{e}\n{source}</code></pre>'