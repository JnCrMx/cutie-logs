# Outgoing IP Filter

The `--outgoing-ip-filter` option accepts a comma-separated list of IPv4 and IPv6 addresses or subnets used to restrict outgoing network connections (e.g., for notifications).

## Rule Syntax
- **Notation:** You can specify single IP addresses or entire subnets using CIDR notation (e.g., `192.168.1.0/24`). If no subnet mask is provided, it defaults to a single IP (`/32` for IPv4, `/128` for IPv6).
- **Actions:** Rules can optionally be prefixed with a specific character to dictate the action:
  - **Deny (`-`):** Blocks the IP or subnet (e.g., `-10.0.0.0/8`). **This is the default action if no prefix is specified**.
  - **Allow (`+`):** Explicitly permits the IP or subnet (e.g., `+192.168.1.50`).
- **Formatting:** Leading and trailing whitespace around each rule is automatically ignored.

## Argument Precedence
The IP filter can be provided in three different ways:
1. the default filter string (see below)
2. via the `CUTIE_LOGS_OUTGOING_IP_FILTER` environment variable
3. via the `--outgoing-ip-filter` command line argument
Later arguments override earlier ones.
If -- for example -- `--outgoing-ip-filter` is provided, it overrides any filter specified by `CUTIE_LOGS_OUTGOING_IP_FILTER`.

However, if a filter contains `...`, this character sequence is replaced with the the previous filter string before parsing.
This can be useful for extended the default filter string rather than replacing it.

### Examples
|`CUTIE_LOGS_OUTGOING_IP_FILTER`|`--outgoing-ip-filter`|final filter string|
|---|---|---|
|*none*|*none*|`<default>`|
|`1.1.1.1`|*none*|`1.1.1.1`|
|*none*|`1.1.1.1`|`1.1.1.1`|
|`1.1.1.1`|`2.2.2.2`|`2.2.2.2`|
|`1.1.1.1`|`..., 2.2.2.2`|`1.1.1.1, 2.2.2.2`|
|`1.1.1.1`|`2.2.2.2, ...`|`2.2.2.2, 1.1.1.1`|
|`1.1.1.1`|`2.2.2.2, ..., 3.3.3.3`|`2.2.2.2, 1.1.1.1, 3.3.3.3`|
|`1.1.1.1, ...`|`2.2.2.2`|`2.2.2.2`|
|*none*|`1.1.1.1, ...`|`1.1.1.1, <default>`|
|`1.1.1.1, ...`|`..., 2.2.2.2`|`1.1.1.1, <default>, 2.2.2.2`|
|*set, but empty*|*none*|*empty*|
|*any*|*set, but empty*|*empty*|

## Evaluation Logic
The final filter string is evaluated sequentially from left to right. The **first rule** that matches the outgoing IP address is applied, and the evaluation stops.

* If an IP address matches an allow rule, the connection proceeds.
* If an IP address matches a deny rule, the connection is blocked.
* If an IP address **does not match any rule** in the filter list, it is allowed by default.
* Invalid or unresolvable addresses fail closed (blocked).

## Built-in default filter string

```text
10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, 127.0.0.0/8, 169.254.0.0/16, 0.0.0.0/8, 100.64.0.0/10, 224.0.0.0/4, 240.0.0.0/4, 255.255.255.255/32, ::1/128, fc00::/7, fe80::/10, ::/128, ::ffff:0:0/96, ff00::/8
```

## Examples
- Allow one specific local IP, but keep the default filter: `+192.168.1.50, ...`
- Allow only specific IPs, block everything else: `+10.0.1.0, -0.0.0.0/0`
