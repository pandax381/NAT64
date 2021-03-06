---
language: es
layout: default
category: Documentation
title: --blacklist
---

[Documentación](documentation.html) > [Aplicación de Espacio de Usuario](documentation.html#aplicacin-de-espacio-de-usuario) > `--blacklist`

# \--blacklist

## Índice

1. [Descripción](#descripcin)
2. [Sintaxis](#sintaxis)
3. [Argumentos](#argumentos)
   1. [Operaciones](#operaciones)
   2. [Opciones](#opciones)
4. [Ejemplos](#ejemplos)

## Descripción

Interactúa con el pool de direcciones que se encuentran en la lista negra de Jool.

El pool dicta qué direcciones pueden ser traducidas utilizando el prefijo [pool6](usr-flags-pool6.html). [EAM](usr-flags-eamt.html) tiene más prioridad que el prefijo, de modo que no es necesario agregar un registro a este pool para cada registro EAMT.

Hay algunas direcciones que Jool se reusará a traducir, independientemente de `blacklist`. Estas incluyen

- Las direcciones que pertenecen al nodo de Jool.
- Direcciones de Software (0.0.0.0/8).
- Direcciones de Host (127.0.0.0/8).
- Direcciones de enlace local (169.254.0.0/16).
- Multicast (224.0.0.0/4).
- Broadcast limitado (255.255.255.255/32).

## Sintaxis

	jool_siit --blacklist (
		[--display] [--csv]
		| --count
		| --add <prefijo-IPv4>
		| --remove <prefijo-IPv4>
		| --flush
	)

## Argumentos

### Operaciones

* `--display`: Los prefijos de las direcciones de blacklist son impresos en salida estándar. Esta es la operación por defecto.
* `--count`: El número de _direcciones_ (no prefijos) en el pool es impreso en salida estándar.  
  Por ejemplo, si todo lo que tienes es un prefijo /30, espera "4" como resultado.
* `--add`: Carga `<prefijo-IPv4>` al pool.
* `--remove`: Borra la dirección `<prefijo-IPv4>` del pool.
* `--flush`: Remueve todas las direcciones/prefijos del pool.

### Opciones

| **Bandera** | **Descripción** |
| `--csv` | Imprimir la tabla en formato [CSV](https://es.wikipedia.org/wiki/CSV). La idea es redireccionar esto a un archivo .csv. |

## Ejemplos

Desplegar las direcciones actuales:

	$ jool_siit --blacklist --display
	192.0.2.0/28
	198.51.100.0/30
	203.0.113.8/32
	  (Fetched 3 prefixes.)

Desplegar el conteo de direcciones:

	$ jool_siit --blacklist --count
	21

(Eso es /28 + /30 + /32 = 16 + 4 + 1)

Remover un par de entradas:

	# jool_siit --blacklist --remove 192.0.2.0/28
	# jool_siit --blacklist --remove 198.51.100.0/30

Devolver una entrada:

	# jool_siit --blacklist --add 192.0.2.0/28

