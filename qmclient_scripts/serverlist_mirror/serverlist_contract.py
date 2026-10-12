"""官方快照与镜像共用的客户端消费边界；不裁切原始列表。"""
import ipaddress
import json
from urllib.parse import urlsplit
MAX_BODY = 8 * 1024 * 1024
LOCATION_PREFIXES = ('an', 'af', 'as', 'oc', 'eu', 'na', 'sa')

def _text(value):
    return isinstance(value, str) and all(ord(char) >= 32 for char in value)

def _integer(value):
    return type(value) is int and -(2**31) <= value < 2**31

def _usable_info(info):
    if not isinstance(info, dict):
        return False
    if not all(_integer(info.get(key)) for key in ('max_clients', 'max_players')):
        return False
    if type(info.get('passworded')) is not bool or not all(_text(info.get(key)) for key in ('name', 'game_type', 'version')):
        return False
    if not isinstance(info.get('map'), dict) or not _text(info['map'].get('name')):
        return False
    if info.get('client_score_kind') is not None and not isinstance(info['client_score_kind'], str):
        return False
    clients = info.get('clients')
    if not isinstance(clients, list):
        return False
    players = 0
    for client in clients:
        if not isinstance(client, dict) or not all(_text(client.get(key)) for key in ('name', 'clan')):
            return False
        if not all(_integer(client.get(key)) for key in ('country', 'score')) or type(client.get('is_player')) is not bool:
            return False
        players += client['is_player']
    return 0 <= players <= info['max_players'] <= info['max_clients'] and len(clients) <= info['max_clients']

def _usable_address(address):
    if not address.startswith(('tw-0.6+udp://', 'tw-0.7+udp://')):
        return False
    try:
        parsed = urlsplit(address)
        if parsed.scheme not in ('tw-0.6+udp', 'tw-0.7+udp') or not parsed.hostname or not parsed.port:
            return False
        if '%' in parsed.hostname:
            return False
        ipaddress.ip_address(parsed.hostname)
        return True
    except ValueError:
        return False

def validate_document(document):
    """返回可消费服务器数；结构错误或全部不可用抛 ValueError，不改正文。"""
    if not isinstance(document, dict) or not isinstance(document.get('servers'), list):
        raise ValueError('missing servers array')
    usable = 0
    for server in document['servers']:
        if not isinstance(server, dict) or not isinstance(server.get('addresses'), list):
            raise ValueError('invalid server addresses structure')
        location = server.get('location')
        if location is not None and (not isinstance(location, str) or not location.startswith(LOCATION_PREFIXES)):
            raise ValueError('invalid server location')
        # 生产解析器在 info 不可消费时跳过，随后才检查地址元素类型。
        if not _usable_info(server.get('info')):
            continue
        addresses = server['addresses']
        if not all(isinstance(address, str) for address in addresses):
            raise ValueError('invalid address element type')
        has_six = any(address.startswith('tw-0.6+udp://') for address in addresses)
        if any(_usable_address(address) for address in addresses if not (has_six and address.startswith('tw-0.7+udp://'))):
            usable += 1
    if not usable:
        raise ValueError('no client-consumable servers')
    return usable

def validate_serverlist(body):
    if not isinstance(body, (bytes, str)):
        raise ValueError('serverlist body must be bytes or str')
    if (len(body) if isinstance(body, bytes) else len(body.encode('utf-8'))) > MAX_BODY:
        raise ValueError('serverlist exceeds 8 MiB')
    try:
        document = json.loads(body)
    except (ValueError, UnicodeError) as error:
        raise ValueError('invalid serverlist JSON') from error
    return validate_document(document)
