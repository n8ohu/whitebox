/***********************************************************************
 * This file was generated on {{ time }}
 **********************************************************************/

#include "{{ name }}.h"

{% block init %}
void {{ name }}_init({{ name }}_t* model) {
    {% for bit_field in model.bit_fields %}
    model->{{ bit_field.name }} = {{ bit_field.default }};
    {% endfor %}
}
{% endblock %}

{% block destroy %}
void {{ name }}_destroy({{ name }}_t* model) {
}
{% endblock %}

{% block copy %}
void {{ name }}_copy({{ name }}_t* src, {{ name }}_t* dest) {
    {% for bit_field in model.bit_fields %}
    dest->{{ bit_field.name }} = src->{{ bit_field.name }};
    {% endfor %}
}
{% endblock %}

{% block print %}
void {{ name }}_print_to_file({{ name }}_t* model, FILE* f) {
    {% for bit_field in model.bit_fields %}
        {% if bit_field.enum %}
            {% for enum in bit_field.enum %}
    if (model->{{ bit_field.name }} == {{ bit_field.name | upper }}_{{ enum[0] | upper }}) fprintf(f, "{{ bit_field.name }}={{ enum[0] | upper }}\n");
            {% endfor %}
        {% else %}
    fprintf(f, "{{ bit_field.name }}=%d\n", model->{{ bit_field.name }});
        {% endif %}
    {% endfor %}

    {% for register in model.registers %}
    fprintf(f, "{{ register.name }}=%d\n", {{ name }}_get_{{ register.name }}(model));
    {% endfor %}
}
{% endblock %}

{% block registers %}
    {% for register in model.registers %}
{{ register.type }} {{ name }}_get_{{ register.name }}({{ name }}_t* model) {
    return
        {% set shift = 0 %}
        {% for bit_field in register.bit_fields %}
        (({{ register.type }})(model->{{ bit_field.name }} & {{ bit_field.mask }}) << {{ shift }}) |
            {% set shift = shift + bit_field.bit_width %}
        {% endfor %}
        0;
}

void {{ name }}_set_{{ register.name }}({{ name }}_t* model, {{ register.type }} value) {
    {% set shift = 0 %}
    {% for bit_field in register.bit_fields %}
    model->{{ bit_field.name }} = (value >> {{ shift }}) & {{ bit_field.mask }};
        {% set shift = shift + bit_field.bit_width %}
    {% endfor %}
}
    {% endfor %}
{% endblock %}
