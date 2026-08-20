EVENT_TYPES = {
    'gunshot':   'Gunshot',
    'artillery': 'Artillery',
    'drone':     'Drone',
}

# Which metadata field drives the primary folder/filename prefix
PRIMARY_KEY = {
    'gunshot':   'caliber',
    'artillery': 'caliber',
    'drone':     'drone_model',
}

# Per-type extra metadata fields: (key, label, default_or_[options_list])
METADATA_FIELDS = {
    'gunshot': [
        ('guntype',    'Weapon type',   ''),
        ('caliber',    'Caliber',       '556'),
        ('distance',   'Distance [m]',  ''),
        ('suppressor', 'Suppressor',    ['no suppressor', 'with suppressor']),
        ('angle',      'Angle [°]',     '0'),
    ],
    'artillery': [
        ('weapon_type', 'Weapon type',  ''),
        ('caliber',     'Caliber',      '152mm'),
        ('distance',    'Distance [m]', ''),
        ('angle',       'Angle [°]',    '0'),
    ],
    'drone': [
        ('drone_model', 'Drone model',  ''),
        ('drone_type',  'Drone type',   ''),
        ('distance',    'Distance [m]', ''),
        ('altitude',    'Altitude [m]', ''),
    ],
}

# Fields that are optional (shown in expander on setup screen)
OPTIONAL_FIELD_KEYS = {
    'gunshot':   {'distance', 'angle'},
    'artillery': {'distance', 'angle'},
    'drone':     {'distance', 'altitude'},
}
