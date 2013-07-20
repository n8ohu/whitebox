/***********************************************************************
 * This file was generated on {{ time }}
 **********************************************************************/
#ifndef __INCLUDE_{{ name | upper }}_H__
#define __INCLUDE_{{ name | upper }}_H__

#include <stdint.h>
#include <stdio.h>

{% block enums %}
    {% for bit_field in model.bit_fields %}
        {% if bit_field.enum %}
typedef enum {
            {% for enum in bit_field.enum %}
                {% set end_comma = ',' if not loop.last else '' %}
    {{ bit_field.name | upper }}_{{ enum[0] | upper }} = {{ enum[1] }}{{ end_comma }}
            {% endfor %}
} {{ bit_field.type }};
        {% endif %}
    {% endfor %}
{% endblock %}

{% block struct %}
typedef struct {
    {% for bit_field in model.bit_fields %}
    {{ bit_field.type }} {{ bit_field.name }};
    {% endfor %}
} {{ name }}_t;

void {{ name }}_init({{ name }}_t*);
void {{ name }}_destroy({{ name }}_t*);
void {{ name }}_copy({{ name }}_t*, {{ name }}_t*);
void {{ name }}_print_to_file({{ name }}_t*, FILE*);
{% endblock %}

{% block registers %}
    {% for register in model.registers %}
{{ register.type }} {{ name }}_get_{{ register.name }}({{ name }}_t*);
void {{ name }}_set_{{ register.name }}({{ name }}_t*, {{ register.type }});
    {% endfor %}
{% endblock %}

{% block custom %}{% endblock %}

#endif /* __INCLUDE_{{ name | upper }}_H__ */
