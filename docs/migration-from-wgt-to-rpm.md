# Migration from wgtpkg to afmpkg

The widget standard is now obsolete and the way it is
integration within DNF/RPM is not using zip package file.

the new packagin doesn't rely any more on the configuration
file `config.xml` but exploit files of directory `.rpconfig`.

These files are:

* `manifest.yml`: description of the application
* `signature-author.p7`: signature of the author if present
* `signature-XXX.p7`: signature of distributors if any

A specific program has been created to migrate `config.xml`
to `manifest.yml`.

The program is called `wgt-migrate`. It is called like below:

```
wgt-migrate FILE > RESULT
```

Where:

* FILE is the path to the configuration file to convert
* RESULT is the path of the created manifest file
