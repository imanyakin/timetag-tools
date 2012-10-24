import logging
import os
from copy import deepcopy
import json
from collections import namedtuple

StrobeChannel = namedtuple('StrobeChannel', 'enabled,color,label')
DeltaChannel = namedtuple('DeltaChannel', 'enabled,label')

default_rc = {
    'strobe-channels': [
        (True,  '#A80505', 'Strobe 1'),
        (True,  '#006620', 'Strobe 2'),
        (False, '#0142D5', 'Strobe 3'),
        (False, '#9030FF', 'Strobe 4'),
        ],
    'delta-channels': [
        (False, 'Delta 1'),
        (False, 'Delta 2'),
        (False, 'Delta 3'),
        (False, 'Delta 4'),
        ],
    }

rc_path = os.path.expanduser('~/.timetagrc')

def load_rc():
    rc = deepcopy(default_rc)
    if os.path.isfile(rc_path):
        try:
            rc.update(json.load(open(rc_path)))
        except Exception as e:
            logging.warn('Warning: Failed to load RC: %s' % e)
    else:
        logging.info("%s doesn't exist, creating..." % rc_path)
        save_rc(rc)

    rc['strobe-channels'] = map(lambda ch: StrobeChannel(*ch), rc['strobe-channels'])
    rc['delta-channels'] = map(lambda ch: DeltaChannel(*ch), rc['delta-channels'])

    return rc

def save_rc(rc):
    json.dump(rc, open(rc_path, 'w'))
    logging.info('Saving %s' % rc_path)

def update_rc(updates):
    rc = load_rc()
    rc.update(updates)
    save_rc(rc)

