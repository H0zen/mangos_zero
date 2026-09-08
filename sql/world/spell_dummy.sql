-- What a spell whose effect describes nothing does.
--
-- Effect 3 carries no description of its own: the row says only that the server
-- is expected to know this spell. For the spells whose whole doing is "throw
-- another spell", that knowledge is these rows.
--
-- A spell may have several rows. Every row whose conditions the cast meets goes
-- into a draw weighted by `weight`, and the one drawn is thrown. One row that
-- passes is therefore always thrown; five rows of equal weight are one chance in
-- five each.
--
-- `caster` and `target` name who throws the spell and who catches it:
--   0  the caster of this spell
--   1  the unit this spell landed on
-- A row naming the unit it landed on is skipped when there is none.
--
-- `needs` is a mask of what else must be true:
--   0x01  the caster is a player
--   0x02  the unit it landed on is a player
--   0x04  the unit it landed on is a creature
--   0x08  this spell was cast from an item
--   0x10  the unit it landed on draws on mana
--   0x20  there is a unit it landed on, even where the row does not name one
--
-- `gender` is that of the unit named by `target`: 0 anyone, 1 male, 2 female.
-- `no_aura` is a spell the unit it landed on must not be carrying, 0 for none.
-- `carries_item` says the thrown spell is charged to the item this one came from.

DROP TABLE IF EXISTS `spell_dummy`;

CREATE TABLE `spell_dummy` (
  `entry`         MEDIUMINT UNSIGNED NOT NULL DEFAULT '0',
  `trigger_spell` MEDIUMINT UNSIGNED NOT NULL DEFAULT '0',
  `weight`        TINYINT UNSIGNED   NOT NULL DEFAULT '1',
  `caster`        TINYINT UNSIGNED   NOT NULL DEFAULT '0',
  `target`        TINYINT UNSIGNED   NOT NULL DEFAULT '0',
  `needs`         MEDIUMINT UNSIGNED NOT NULL DEFAULT '0',
  `gender`        TINYINT UNSIGNED   NOT NULL DEFAULT '0',
  `no_aura`       MEDIUMINT UNSIGNED NOT NULL DEFAULT '0',
  `carries_item`  TINYINT UNSIGNED   NOT NULL DEFAULT '0',
  `comment`       TEXT,
  PRIMARY KEY (`entry`, `trigger_spell`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='What a spell with no description of its own throws';

INSERT INTO `spell_dummy`
  (`entry`, `trigger_spell`, `weight`, `caster`, `target`, `needs`, `gender`, `no_aura`, `carries_item`, `comment`) VALUES
(3360, 10651, 1, 0, 1, 0x00, 1, 0, 0, 'Curse of the Eye - on a man'),
(3360, 10653, 1, 0, 1, 0x00, 2, 0, 0, 'Curse of the Eye - on a woman'),
(7671, 24085, 1, 1, 1, 0x00, 0, 0, 0, 'Transformation - the visual it wears'),
(8063,  8064, 1, 0, 0, 0x01, 0, 0, 0, 'Deviate Fish - Sleepy'),
(8063,  8065, 1, 0, 0, 0x01, 0, 0, 0, 'Deviate Fish - Invigorate'),
(8063,  8066, 1, 0, 0, 0x01, 0, 0, 0, 'Deviate Fish - Shrink'),
(8063,  8067, 1, 0, 0, 0x01, 0, 0, 0, 'Deviate Fish - Party Time!'),
(8063,  8068, 1, 0, 0, 0x01, 0, 0, 0, 'Deviate Fish - Healthy Spirit'),
(8213,  8219, 1, 0, 0, 0x01, 1, 0, 0, 'Savory Deviate Delight - Flip Out, ninja'),
(8213,  8220, 1, 0, 0, 0x01, 2, 0, 0, 'Savory Deviate Delight - Flip Out, ninja'),
(8213,  8221, 1, 0, 0, 0x01, 1, 0, 0, 'Savory Deviate Delight - Yaaarrrr, pirate'),
(8213,  8222, 1, 0, 0, 0x01, 2, 0, 0, 'Savory Deviate Delight - Yaaarrrr, pirate'),
(8344,  8345, 1, 0, 1, 0x08, 0, 0, 1, 'Gnomish Universal Remote - control the machine'),
(8344,  8346, 1, 0, 1, 0x08, 0, 0, 1, 'Gnomish Universal Remote - root the machine'),
(8344,  8347, 1, 0, 1, 0x08, 0, 0, 1, 'Gnomish Universal Remote - enrage the machine'),
(13278, 13493, 1, 0, 0, 0x20, 0, 0, 0, 'Gnomish Death Ray charging'),
(13489, 14744, 1, 1, 1, 0x00, 0, 0, 0, 'Unstable Power'),
(16589, 16591, 1, 0, 0, 0x01, 0, 0, 0, 'Noggenfogger Elixir - skeleton'),
(16589, 16593, 1, 0, 0, 0x01, 0, 0, 0, 'Noggenfogger Elixir - slow fall'),
(16589, 16595, 1, 0, 0, 0x01, 0, 0, 0, 'Noggenfogger Elixir - shrink'),
(17770, 29940, 1, 0, 0, 0x00, 0, 0, 0, 'Wolfshead Helm Energy'),
(17950, 17863, 1, 0, 1, 0x00, 0, 0, 0, 'Shadow Portal'),
(17950, 17939, 1, 0, 1, 0x00, 0, 0, 0, 'Shadow Portal'),
(17950, 17943, 1, 0, 1, 0x00, 0, 0, 0, 'Shadow Portal'),
(17950, 17944, 1, 0, 1, 0x00, 0, 0, 0, 'Shadow Portal'),
(17950, 17946, 1, 0, 1, 0x00, 0, 0, 0, 'Shadow Portal'),
(17950, 17948, 1, 0, 1, 0x00, 0, 0, 0, 'Shadow Portal'),
(19395, 19394, 1, 1, 1, 0x02, 0, 0, 0, 'Gordunni Trap'),
(19395, 11756, 1, 1, 1, 0x02, 0, 0, 0, 'Gordunni Trap'),
(19411, 20494, 1, 1, 1, 0x00, 0, 0, 0, 'Lava Bomb'),
(20474, 20494, 1, 1, 1, 0x00, 0, 0, 0, 'Lava Bomb'),
(19869, 19832, 1, 1, 1, 0x02, 0, 23958, 0, 'Dragon Orb - not for one already blessed'),
(20037, 20038, 1, 1, 1, 0x00, 0, 0, 0, 'Explode Orb Effect'),
(23074, 19804, 1, 0, 0, 0x08, 0, 0, 1, 'Arcanite Dragonling'),
(23075, 12749, 1, 0, 0, 0x08, 0, 0, 1, 'Mithril Mechanical Dragonling'),
(23076,  4073, 1, 0, 0, 0x08, 0, 0, 1, 'Mechanical Dragonling'),
(23133, 13166, 1, 0, 0, 0x08, 0, 0, 1, 'Gnomish Battle Chicken'),
(23138, 23139, 1, 0, 1, 0x00, 0, 0, 0, 'Gate of Shazzrah'),
(24930, 24924, 1, 0, 0, 0x00, 0, 0, 0, 'Hallow''s End Treat - larger and orange'),
(24930, 24925, 1, 0, 0, 0x00, 0, 0, 0, 'Hallow''s End Treat - skeleton'),
(24930, 24926, 1, 0, 0, 0x00, 0, 0, 0, 'Hallow''s End Treat - pirate'),
(24930, 24927, 1, 0, 0, 0x00, 0, 0, 0, 'Hallow''s End Treat - ghost'),
(26626, 25779, 1, 0, 1, 0x14, 0, 0, 0, 'Mana Burn Area'),
(28006, 29294, 1, 0, 1, 0x02, 0, 0, 0, 'Arcane Cloaking - Naxxramas entry flag');
